// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "components/nacl/loader/nonsfi/abi_conversion.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "components/nacl/loader/nonsfi/irt_util.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"

namespace nacl {
namespace nonsfi {
namespace {

void IrtExit(int status) {
  _exit(status);
}

int IrtGetToD(struct nacl_abi_timeval* tv) {
  struct timeval host_tv;
  if (gettimeofday(&host_tv, NULL))
    return errno;
  tv->nacl_abi_tv_sec = host_tv.tv_sec;
  tv->nacl_abi_tv_usec = host_tv.tv_usec;
  return 0;
}

int IrtClock(nacl_abi_clock_t* ticks) {
  // There is no definition of errno when clock is failed.
  // So we assume it always succeeds.
  *ticks = clock();
  return 0;
}

int IrtNanoSleep(const struct nacl_abi_timespec* req,
                 struct nacl_abi_timespec* rem) {
  struct timespec host_req;
  NaClAbiTimeSpecToTimeSpec(*req, &host_req);
  struct timespec host_rem;
  if (nanosleep(&host_req, &host_rem))
    return errno;

  if (rem)
    TimeSpecToNaClAbiTimeSpec(host_rem, rem);
  return 0;
}

int IrtSchedYield() {
  return CheckError(sched_yield());
}

int IrtSysconf(int name, int* value) {
  int result;
  switch (name) {
    case NACL_ABI__SC_NPROCESSORS_ONLN:
      errno = 0;
      result = sysconf(_SC_NPROCESSORS_ONLN);
      break;
    case NACL_ABI__SC_PAGESIZE:
      errno = 0;
      result = sysconf(_SC_PAGESIZE);
      break;
    default:
      return EINVAL;
  }

  if (result == -1 && errno == EINVAL)
    return EINVAL;

  *value = result;
  return 0;
}

}  // namespace

// For gettod, clock and nanosleep, their argument types should be nacl_abi_X,
// rather than host type, such as timeval or clock_t etc. However, the
// definition of nacl_irt_basic uses host types, so here we need to cast them.
const nacl_irt_basic kIrtBasic = {
  IrtExit,
  reinterpret_cast<int(*)(struct timeval*)>(IrtGetToD),
  reinterpret_cast<int(*)(clock_t*)>(IrtClock),
  reinterpret_cast<int(*)(const struct timespec*, struct timespec*)>(
      IrtNanoSleep),
  IrtSchedYield,
  IrtSysconf,
};

}  // namespace nonsfi
}  // namespace nacl
