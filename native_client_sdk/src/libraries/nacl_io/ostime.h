/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_OSTIME_H_
#define LIBRARIES_NACL_IO_OSTIME_H_

#if defined(WIN32)

#include <pthread.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME (clockid_t)1
#endif

int clock_gettime(clockid_t clock_id, struct timespec* tp);
int clock_settime(clockid_t clock_id, const struct timespec* tp);

#else

#include <time.h>

#endif

#endif  // LIBRARIES_NACL_IO_OSUNISTD_H_
