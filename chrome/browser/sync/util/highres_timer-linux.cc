// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// High resolution timer functions for use in Linux.

#include "chrome/browser/sync/util/highres_timer.h"

const uint64 MICROS_IN_MILLI = 1000L;
const uint64 MICROS_IN_HALF_MILLI = 500L;
const uint64 MICROS_IN_HALF_SECOND = 500000L;

uint64 HighresTimer::GetElapsedMs() const {
  uint64 end_time = GetCurrentTicks();

  // Scale to ms and round to nearest ms - rounding is important because
  // otherwise the truncation error may accumulate e.g. in sums.
  return (uint64(end_time - start_ticks_) + MICROS_IN_HALF_MILLI) /
             MICROS_IN_MILLI;
}

uint64 HighresTimer::GetElapsedSec() const {
  uint64 end_time = GetCurrentTicks();

  // Scale to ms and round to nearest ms - rounding is important because
  // otherwise the truncation error may accumulate e.g. in sums.
  return (uint64(end_time - start_ticks_) + MICROS_IN_HALF_SECOND) /
             MICROS_IN_SECOND;
}
