// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// High resolution timer unit tests.

#include "base/basictypes.h"
#include "build/build_config.h"
#include "chrome/browser/sync/util/highres_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

// These unittests have proven to be flaky on buildbot. While we don't want
// them breaking the build, we still build them to guard against bitrot.
// On dev machines during local builds we can enable them.
TEST(HighresTimer, DISABLED_MillisecondClock) {
  HighresTimer timer;

  // Note: this could fail if we context switch between initializing the timer
  // and here. Very unlikely however.
  EXPECT_EQ(0, timer.GetElapsedMs());
  timer.Start();
  uint64 half_ms = HighresTimer::GetTimerFrequency() / 2000;
  // Busy wait for half a millisecond.
  while (timer.start_ticks() + half_ms > HighresTimer::GetCurrentTicks()) {
    // Nothing
  }
  EXPECT_EQ(1, timer.GetElapsedMs());
}

TEST(HighresTimer, DISABLED_SecondClock) {
  HighresTimer timer;

  EXPECT_EQ(0, timer.GetElapsedSec());
#ifdef OS_WIN
  ::Sleep(250);
#else
  struct timespec ts1 = {0, 250000000};
  nanosleep(&ts1, 0);
#endif
  EXPECT_EQ(0, timer.GetElapsedSec());
  EXPECT_LE(230, timer.GetElapsedMs());
  EXPECT_GE(270, timer.GetElapsedMs());
#ifdef OS_WIN
  ::Sleep(251);
#else
  struct timespec ts2 = {0, 251000000};
  nanosleep(&ts2, 0);
#endif
  EXPECT_EQ(1, timer.GetElapsedSec());
}
