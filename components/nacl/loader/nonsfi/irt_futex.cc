// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "components/nacl/loader/nonsfi/irt_util.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"

namespace nacl {
namespace nonsfi {
namespace {

// Converts a pair of NaCl's timespec of absolute time and host's timespec of
// the current time to host's timespec of the relative time between them.
// Note that assuming tv_nsec for the both input structs are non-negative,
// the returned reltime's tv_nsec is also always non-negative.
// (I.e., for -0.1 sec, {tv_sec: -1, tv_nsec: 900000000} will be returned).
void NaClAbsTimeToRelTime(const struct nacl_abi_timespec& nacl_abstime,
                          const struct timespec& now,
                          struct timespec* reltime) {
  reltime->tv_sec = nacl_abstime.tv_sec - now.tv_sec;
  reltime->tv_nsec = nacl_abstime.tv_nsec - now.tv_nsec;
  if (reltime->tv_nsec < 0) {
    reltime->tv_sec -= 1;
    reltime->tv_nsec += 1000000000;
  }
}

int IrtFutexWaitAbs(volatile int* addr, int value,
                    const struct nacl_abi_timespec* abstime) {
  struct timespec timeout;
  struct timespec* timeout_ptr = NULL;
  if (abstime) {
    // futex syscall takes relative timeout, but the ABI for IRT's
    // futex_wait_abs is absolute timeout. So, here we convert it.
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now))
      return errno;
    NaClAbsTimeToRelTime(*abstime, now, &timeout);

    // Linux's FUTEX_WAIT returns EINVAL for negative timeout, but an absolute
    // time that is in the past is a valid argument to irt_futex_wait_abs(),
    // and a caller expects ETIMEDOUT.
    // Here check only tv_sec since we assume time_t is signed. See also
    // the comment for NaClAbsTimeToRelTime.
    if (timeout.tv_sec < 0)
      return ETIMEDOUT;
    timeout_ptr = &timeout;
  }
  return CheckError(
      syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, value, timeout_ptr, 0, 0));
}

int IrtFutexWake(volatile int* addr, int nwake, int* count) {
  return CheckErrorWithResult(
      syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, nwake, 0, 0, 0), count);
}

}  // namespace

// For futex_wait_abs, the argument type should be nacl_abi_timespec. However,
// the definition uses its host type, struct timespec. So, here we need to cast
// it.
const nacl_irt_futex kIrtFutex = {
  reinterpret_cast<int(*)(volatile int*, int, const struct timespec*)>(
      IrtFutexWaitAbs),
  IrtFutexWake,
};

}  // namespace nonsfi
}  // namespace nacl
