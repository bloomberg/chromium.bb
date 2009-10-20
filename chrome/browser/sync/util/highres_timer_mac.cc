// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// High resolution timer functions for use on Mac OS.

#include "chrome/browser/sync/util/highres_timer.h"

bool HighresTimer::perf_ratio_collected_ = false;
mach_timebase_info_data_t HighresTimer::perf_ratio_ = {0};

static const uint64 kNanosInMilli = 1000000L;
static const uint64 kNanosInHalfMilli = 500000L;
static const uint64 kNanosInSecond = 1000000000L;
static const uint64 kNanosInHalfSecond = 500000000L;

uint64 HighresTimer::GetElapsedMs() const {
  uint64 end_time = GetCurrentTicks();

  // Scale to ms and round to nearest ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  GetTimerFrequency();
  return ((end_time - start_ticks_) * perf_ratio_.numer +
            kNanosInHalfMilli * perf_ratio_.denom) /
            (kNanosInMilli * perf_ratio_.denom);
}

uint64 HighresTimer::GetElapsedSec() const {
  uint64 end_time = GetCurrentTicks();

  // Scale to ms and round to nearest ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  GetTimerFrequency();
  return ((end_time - start_ticks_) * perf_ratio_.numer +
            kNanosInHalfSecond * perf_ratio_.denom) /
            (kNanosInSecond * perf_ratio_.denom);
}

void HighresTimer::CollectPerfRatio() {
  mach_timebase_info(&perf_ratio_);
  perf_ratio_collected_ = true;
}

uint64 HighresTimer::GetTimerFrequency() {
    if (!perf_ratio_collected_)
        CollectPerfRatio();
    // we're losing precision by doing the division here, but this value is only
    // used to estimate tick time by the unit tests, so we're ok.
    return static_cast<uint64>(
        perf_ratio_.denom * 1000000000ULL / perf_ratio_.numer);
}
