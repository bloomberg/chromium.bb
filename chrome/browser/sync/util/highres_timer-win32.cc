// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// High resolution timer functions for use in Windows.

#include "chrome/browser/sync/util/highres_timer.h"

bool HighresTimer::perf_freq_collected_ = false;
ULONGLONG HighresTimer::perf_freq_ = 0;

ULONGLONG HighresTimer::GetElapsedMs() const {
  ULONGLONG end_time = GetCurrentTicks();

  // Scale to ms and round to nearest ms - rounding is important because
  // otherwise the truncation error may accumulate e.g. in sums.
  //
  // Given infinite resolution, this expression could be written as:
  //  trunc((end - start (units:freq*sec))/freq (units:sec) *
  //                1000 (unit:ms) + 1/2 (unit:ms))
  ULONGLONG freq = GetTimerFrequency();
  return ((end_time - start_ticks_) * 1000L + freq / 2) / freq;
}

ULONGLONG HighresTimer::GetElapsedSec() const {
  ULONGLONG end_time = GetCurrentTicks();

  // Round to nearest ms - rounding is important because otherwise the
  // truncation error may accumulate e.g. in sums.
  //
  // Given infinite resolution, this expression could be written as:
  //  trunc((end - start (units:freq*sec))/freq (unit:sec) + 1/2 (unit:sec))
  ULONGLONG freq = GetTimerFrequency();
  return ((end_time - start_ticks_) + freq / 2) / freq;
}

void HighresTimer::CollectPerfFreq() {
  LARGE_INTEGER freq;

  // Note that this is racy. It's OK, however, because even concurrent
  // executions of this are idempotent.
  if (::QueryPerformanceFrequency(&freq)) {
    perf_freq_ = freq.QuadPart;
    perf_freq_collected_ = true;
  }
}
