// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// High resolution timer functions for use in Linux.

#ifndef CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_LINUX_H_
#define CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_LINUX_H_

#include "base/basictypes.h"

#include <sys/time.h>

const uint64 MICROS_IN_SECOND = 1000000L;

// A handy class for reliably measuring wall-clock time with decent resolution.
//
// We want to measure time with high resolution on Linux. What to do?
//
// RDTSC? Sure, but how do you convert it to wall clock time?
// clock_gettime? It's not in all Linuxes.
//
// Let's just use gettimeofday; it's good to the microsecond.
class HighresTimer {
 public:
  // Captures the current start time.
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
  // Captured start time.
  uint64 start_ticks_;
};

inline HighresTimer::HighresTimer() {
  Start();
}

inline void HighresTimer::Start() {
  start_ticks_ = GetCurrentTicks();
}

inline uint64 HighresTimer::GetTimerFrequency() {
  // Fixed; one "tick" is one microsecond.
  return MICROS_IN_SECOND;
}

inline uint64 HighresTimer::GetCurrentTicks() {
  timeval tv;
  gettimeofday(&tv, 0);

  return tv.tv_sec * MICROS_IN_SECOND + tv.tv_usec;
}

inline uint64 HighresTimer::GetElapsedTicks() const {
  return start_ticks_ - GetCurrentTicks();
}

#endif  // CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_LINUX_H_
