/* tcap.c - check that pthreads can independently set capabilities */

#include <unistd.h>
#include <sys/fsuid.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/capability.h>
#include <grp.h>

#include "diod_log.h"

#include "test.h"

#define TEST_GID 100
#define TEST_GID2 101

typedef enum { S0, S1, S2, S3, S4, S5 } state_t;

static state_t         state = S0;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  state_cond = PTHREAD_COND_INITIALIZER;

static void
_prtcap (char *s, cap_value_t capflag)
{
    cap_t cap;
    cap_flag_value_t val;

    if (!(cap = cap_get_proc ()))
        err_exit ("%s: cap_get_proc", s);
    if (cap_get_flag (cap, capflag, CAP_EFFECTIVE, &val) < 0)
        err_exit ("%s: cap_get_flag", s);
    if (cap_free (cap) < 0)
        err_exit ("%s: cap_free", s);
    msg ("%s: cap is %s", s, val == CAP_SET ? "set" : "clear");
}

static void
_setcap (char *s, cap_value_t capflag)
{
    cap_t cap;

    if (!(cap = cap_get_proc ()))
        err_exit ("%s: cap_get_proc", s);
    if (cap_set_flag (cap, CAP_EFFECTIVE, 1, &capflag, CAP_SET) < 0)
        err_exit ("%s: cap_set_flag", s); 
    if (cap_set_proc (cap) < 0)
        err_exit ("%s: cap_set_proc", s); 
    if (cap_free (cap) < 0)
        err_exit ("%s: cap_free", s);
}

static void
_clrcap (char *s, cap_value_t capflag)
{
    cap_t cap;

    if (!(cap = cap_get_proc ()))
        err_exit ("%s: cap_get_proc", s);
    if (cap_set_flag (cap, CAP_EFFECTIVE, 1, &capflag, CAP_CLEAR) < 0)
        err_exit ("%s: cap_set_flag", s); 
    if (cap_set_proc (cap) < 0)
        err_exit ("%s: cap_set_proc", s); 
    if (cap_free (cap) < 0)
        err_exit ("%s: cap_free", s);
}

static void
change_state (state_t s)
{
    _lock (&state_lock);
    state = s;
    _condsig (&state_cond);
    _unlock (&state_lock);
}

static void
wait_state (state_t s)
{
    _lock (&state_lock);
    while ((state != s))
        _condwait (&state_cond, &state_lock);
    _unlock (&state_lock);
}

static void *proc1 (void *a)
{
    _prtcap ("task1", CAP_DAC_OVERRIDE);
    change_state (S1);

    wait_state (S2);
    _prtcap ("task1", CAP_DAC_OVERRIDE);

    msg ("task1: clr cap");
    _clrcap ("task1", CAP_DAC_OVERRIDE);
    change_state (S3);

    wait_state (S4);
    return NULL;
}

static void *proc2 (void *a)
{
    wait_state (S1);
    _prtcap ("task2", CAP_DAC_OVERRIDE);

    msg ("task2: set cap");
    _setcap ("task2", CAP_DAC_OVERRIDE);
    _prtcap ("task2", CAP_DAC_OVERRIDE);
    change_state (S2);

    wait_state (S3);
    _prtcap ("task2", CAP_DAC_OVERRIDE);
    change_state (S4);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t t1, t2;

    diod_log_init (argv[0]);

    assert (geteuid () == 0);

    _prtcap ("task0", CAP_DAC_OVERRIDE); /* root, expect set */

    msg ("task0: setfsuid 1");
    setfsuid (1); 
    _prtcap ("task0", CAP_DAC_OVERRIDE); /* non-root, expect clr */

    msg ("task0: setfsuid 0");          /* root, expect set */
    setfsuid (0); 
    _prtcap ("task0", CAP_DAC_OVERRIDE);

    msg ("task0: setfsuid 1");
    setfsuid (1); 
    _prtcap ("task0", CAP_DAC_OVERRIDE); /* non-root, expect clr */

    msg ("task0: set cap");
    _setcap ("task0", CAP_DAC_OVERRIDE);
    _prtcap ("task0", CAP_DAC_OVERRIDE);

    msg ("task0: setfsuid 2");
    setfsuid (2); 
    _prtcap ("task0", CAP_DAC_OVERRIDE);

    msg ("task0: clr cap");
    _clrcap ("task0", CAP_DAC_OVERRIDE);
    _prtcap ("task0", CAP_DAC_OVERRIDE);

    _create (&t1, proc1, NULL);
    _create (&t2, proc2, NULL);

    _join (t2, NULL);
    _join (t1, NULL);

    _prtcap ("task0", CAP_DAC_OVERRIDE);

    exit (0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
