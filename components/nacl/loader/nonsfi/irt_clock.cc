// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <time.h>

#include "components/nacl/loader/nonsfi/abi_conversion.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"

namespace nacl {
namespace nonsfi {
namespace {

bool NaClAbiClockIdToClockId(nacl_irt_clockid_t nacl_clk_id,
                             clockid_t* host_clk_id) {
  switch (nacl_clk_id) {
    case NACL_ABI_CLOCK_REALTIME:
      *host_clk_id = CLOCK_REALTIME;
      break;
    case NACL_ABI_CLOCK_MONOTONIC:
      *host_clk_id = CLOCK_MONOTONIC;
      break;
    case NACL_ABI_CLOCK_PROCESS_CPUTIME_ID:
      *host_clk_id = CLOCK_PROCESS_CPUTIME_ID;
      break;
    case NACL_ABI_CLOCK_THREAD_CPUTIME_ID:
      *host_clk_id = CLOCK_THREAD_CPUTIME_ID;
      break;
    default:
      // Unknown clock id.
      return false;
  }
  return true;
}

int IrtClockGetRes(nacl_irt_clockid_t clk_id, struct nacl_abi_timespec* res) {
  clockid_t host_clk_id;
  if (!NaClAbiClockIdToClockId(clk_id, &host_clk_id))
    return EINVAL;

  struct timespec host_res;
  if (clock_getres(host_clk_id, &host_res))
    return errno;

  // clock_getres() requires a NULL check but clock_gettime() doesn't.
  if (res)
    TimeSpecToNaClAbiTimeSpec(host_res, res);
  return 0;
}

int IrtClockGetTime(nacl_irt_clockid_t clk_id, struct nacl_abi_timespec* tp) {
  clockid_t host_clk_id;
  if (!NaClAbiClockIdToClockId(clk_id, &host_clk_id))
    return EINVAL;

  struct timespec host_tp;
  if (clock_gettime(host_clk_id, &host_tp))
    return errno;

  TimeSpecToNaClAbiTimeSpec(host_tp, tp);
  return 0;
}

}  // namespace

// Cast away the distinction between host types and NaCl ABI types.
const nacl_irt_clock kIrtClock = {
  reinterpret_cast<int(*)(nacl_irt_clockid_t, struct timespec*)>(
      IrtClockGetRes),
  reinterpret_cast<int(*)(nacl_irt_clockid_t, struct timespec*)>(
      IrtClockGetTime),
};

}  // namespace nonsfi
}  // namespace nacl
