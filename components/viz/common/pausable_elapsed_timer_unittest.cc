// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/pausable_elapsed_timer.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class PausableElapsedTimerTest : public testing::Test {
 protected:
  base::ScopedMockClockOverride mock_clock_;
};

// The timer starts as paused.
TEST_F(PausableElapsedTimerTest, StartsAsPaused) {
  PausableElapsedTimer timer;

  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(2));

  EXPECT_EQ(timer.Elapsed(), base::TimeDelta::FromMilliseconds(0));
}

// Get the elapsed time while paused, without any pause before.
TEST_F(PausableElapsedTimerTest, ElapsedWhilePaused) {
  PausableElapsedTimer timer;
  timer.Resume();

  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(5));
  timer.Pause();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(5));

  EXPECT_EQ(timer.Elapsed(), base::TimeDelta::FromMilliseconds(5));
}

// Get the elapsed time while unpaused with pausing in between.
TEST_F(PausableElapsedTimerTest, WithPausing) {
  PausableElapsedTimer timer;
  timer.Resume();

  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  timer.Pause();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  timer.Resume();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(3));

  EXPECT_EQ(timer.Elapsed(), base::TimeDelta::FromMilliseconds(5));
}

// Get the elapsed time while paused, with some pauses before.
TEST_F(PausableElapsedTimerTest, ElpasedWhilePausedWithPausesBefore) {
  PausableElapsedTimer timer;
  timer.Resume();

  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  timer.Pause();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  timer.Resume();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(3));
  timer.Pause();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(7));

  EXPECT_EQ(timer.Elapsed(), base::TimeDelta::FromMilliseconds(4));
}

// Pausing while already paused should not change the internal time.
TEST_F(PausableElapsedTimerTest, PausingWhilePaused) {
  PausableElapsedTimer timer;
  timer.Resume();

  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  timer.Pause();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  timer.Pause();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(4));

  EXPECT_EQ(timer.Elapsed(), base::TimeDelta::FromMilliseconds(1));
}

// Resuming while not paused should not change the internal time.
TEST_F(PausableElapsedTimerTest, ResumingWhileNotPaused) {
  PausableElapsedTimer timer;
  timer.Resume();

  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  timer.Pause();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(3));
  timer.Resume();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(5));
  timer.Resume();
  mock_clock_.Advance(base::TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(timer.Elapsed(), base::TimeDelta::FromMilliseconds(16));
}

}  // namespace viz
