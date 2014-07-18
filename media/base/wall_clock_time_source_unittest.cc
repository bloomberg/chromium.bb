// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/base/wall_clock_time_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class WallClockTimeSourceTest : public testing::Test {
 public:
  WallClockTimeSourceTest() : tick_clock_(new base::SimpleTestTickClock()) {
    time_source_.SetTickClockForTesting(
        scoped_ptr<base::TickClock>(tick_clock_));
  }
  virtual ~WallClockTimeSourceTest() {}

  void AdvanceTimeInSeconds(int seconds) {
    tick_clock_->Advance(base::TimeDelta::FromSeconds(seconds));
  }

  int CurrentMediaTimeInSeconds() {
    return time_source_.CurrentMediaTime().InSeconds();
  }

  void SetMediaTimeInSeconds(int seconds) {
    return time_source_.SetMediaTime(base::TimeDelta::FromSeconds(seconds));
  }

  WallClockTimeSource time_source_;

 private:
  base::SimpleTestTickClock* tick_clock_;  // Owned by |time_source_|.

  DISALLOW_COPY_AND_ASSIGN(WallClockTimeSourceTest);
};

TEST_F(WallClockTimeSourceTest, InitialTimeIsZero) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
}

TEST_F(WallClockTimeSourceTest, InitialTimeIsNotTicking) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  AdvanceTimeInSeconds(100);
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
}

TEST_F(WallClockTimeSourceTest, InitialPlaybackRateIsOne) {
  time_source_.StartTicking();

  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  AdvanceTimeInSeconds(100);
  EXPECT_EQ(100, CurrentMediaTimeInSeconds());
}

TEST_F(WallClockTimeSourceTest, SetMediaTime) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  SetMediaTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
}

TEST_F(WallClockTimeSourceTest, SetPlaybackRate) {
  time_source_.StartTicking();

  time_source_.SetPlaybackRate(0.5);
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(5, CurrentMediaTimeInSeconds());

  time_source_.SetPlaybackRate(2);
  EXPECT_EQ(5, CurrentMediaTimeInSeconds());
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(25, CurrentMediaTimeInSeconds());
}

TEST_F(WallClockTimeSourceTest, StopTicking) {
  time_source_.StartTicking();

  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());

  time_source_.StopTicking();

  AdvanceTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
}

}  // namespace media
