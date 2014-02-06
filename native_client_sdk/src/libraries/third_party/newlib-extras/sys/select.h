#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H	1

#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "sdk_util/macros.h"

EXTERN_C_BEGIN

int select (int __nfds, fd_set *__restrict __readfds,
            fd_set *__restrict __writefds,
            fd_set *__restrict __exceptfds,
            struct timeval *__restrict __timeout) __THROW;

EXTERN_C_END

#endif /* sys/select.h */
