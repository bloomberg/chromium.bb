// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// High resolution timer functions for use in Windows.

#ifndef CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_WIN_H_
#define CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_WIN_H_

#include <windows.h>

// A handy class for reliably measuring wall-clock time with decent resolution,
// even on multi-processor machines and on laptops (where RDTSC potentially
// returns different results on different processors and/or the RDTSC timer
// clocks at different rates depending on the power state of the CPU,
// respectively).
class HighresTimer {
 public:
  // Captures the current start time.
  HighresTimer();

  // Captures the current tick, can be used to reset a timer for reuse.
  void Start();

  // Returns the elapsed ticks with full resolution.
  ULONGLONG GetElapsedTicks() const;

  // Returns the elapsed time in milliseconds, rounded to the nearest
  // millisecond.
  ULONGLONG GetElapsedMs() const;

  // Returns the elapsed time in seconds, rounded to the nearest second.
  ULONGLONG GetElapsedSec() const;

  ULONGLONG start_ticks() const { return start_ticks_; }

  // Returns timer frequency from cache, should be less
  // overhead than ::QueryPerformanceFrequency.
  static ULONGLONG GetTimerFrequency();
  // Returns current ticks.
  static ULONGLONG GetCurrentTicks();

 private:
  static void CollectPerfFreq();

  // Captured start time.
  ULONGLONG start_ticks_;

  // Captured performance counter frequency.
  static bool perf_freq_collected_;
  static ULONGLONG perf_freq_;
};

inline HighresTimer::HighresTimer() {
  Start();
}

inline void HighresTimer::Start() {
  start_ticks_ = GetCurrentTicks();
}

inline ULONGLONG HighresTimer::GetTimerFrequency() {
  if (!perf_freq_collected_)
    CollectPerfFreq();
  return perf_freq_;
}

inline ULONGLONG HighresTimer::GetCurrentTicks() {
  LARGE_INTEGER ticks;
  ::QueryPerformanceCounter(&ticks);
  return ticks.QuadPart;
}

inline ULONGLONG HighresTimer::GetElapsedTicks() const {
  return start_ticks_ - GetCurrentTicks();
}

#endif  // CHROME_BROWSER_SYNC_UTIL_HIGHRES_TIMER_WIN_H_
