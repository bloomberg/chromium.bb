// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// High resolution timer functions for use on Mac OS.

#ifndef CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_MAC_H_
#define CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_MAC_H_

#include <mach/mach.h>
#include <mach/mach_time.h>

#include "base/basictypes.h"

// A handy class for reliably measuring wall-clock time with decent resolution.
class HighresTimer {
 public:
  // Captures the current start time
  HighresTimer();

  // Captures the current tick, can be used to reset a timer for reuse.
  void Start();

  // Returns the elapsed ticks with full resolution.
  uint64 GetElapsedTicks() const;

  // Returns the elapsed time in milliseconds, rounded to the nearest
  // millisecond.
  uint64 GetElapsedMs() const;

  // Returns the elapsed time in seconds, rounded to the nearest second.
  uint64 GetElapsedSec() const;

  uint64 start_ticks() const { return start_ticks_; }

  // Returns timer frequency from cache, should be less overhead than
  // ::QueryPerformanceFrequency.
  static uint64 GetTimerFrequency();
  // Returns current ticks.
  static uint64 GetCurrentTicks();

 private:
  static void CollectPerfRatio();

  // Captured start time.
  uint64 start_ticks_;

  // Captured performance counter frequency.
  static bool perf_ratio_collected_;
  static mach_timebase_info_data_t perf_ratio_;
};

inline HighresTimer::HighresTimer() {
  Start();
}

inline void HighresTimer::Start() {
  start_ticks_ = GetCurrentTicks();
}

inline uint64 HighresTimer::GetCurrentTicks() {
  return mach_absolute_time();
}

inline uint64 HighresTimer::GetElapsedTicks() const {
  return start_ticks_ - GetCurrentTicks();
}

#endif  // CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_MAC_H_
