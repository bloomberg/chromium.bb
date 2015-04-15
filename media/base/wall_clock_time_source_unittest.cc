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
    AdvanceTimeInSeconds(1);
  }
  ~WallClockTimeSourceTest() override {}

  void AdvanceTimeInSeconds(int seconds) {
    tick_clock_->Advance(base::TimeDelta::FromSeconds(seconds));
  }

  int CurrentMediaTimeInSeconds() {
    return time_source_.CurrentMediaTime().InSeconds();
  }

  void SetMediaTimeInSeconds(int seconds) {
    return time_source_.SetMediaTime(base::TimeDelta::FromSeconds(seconds));
  }

  bool IsWallClockNowForMediaTimeInSeconds(int seconds) {
    return tick_clock_->NowTicks() ==
           time_source_.GetWallClockTime(base::TimeDelta::FromSeconds(seconds));
  }

  bool IsWallClockNullForMediaTimeInSeconds(int seconds) {
    return time_source_.GetWallClockTime(base::TimeDelta::FromSeconds(seconds))
        .is_null();
  }

 protected:
  WallClockTimeSource time_source_;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |time_source_|.

  DISALLOW_COPY_AND_ASSIGN(WallClockTimeSourceTest);
};

TEST_F(WallClockTimeSourceTest, InitialTimeIsZero) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNullForMediaTimeInSeconds(0));
}

TEST_F(WallClockTimeSourceTest, InitialTimeIsNotTicking) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNullForMediaTimeInSeconds(0));
  AdvanceTimeInSeconds(100);
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNullForMediaTimeInSeconds(0));
}

TEST_F(WallClockTimeSourceTest, InitialPlaybackRateIsOne) {
  time_source_.StartTicking();

  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(0));
  AdvanceTimeInSeconds(100);
  EXPECT_EQ(100, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(100));
}

TEST_F(WallClockTimeSourceTest, SetMediaTime) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNullForMediaTimeInSeconds(0));
  SetMediaTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNullForMediaTimeInSeconds(10));
}

TEST_F(WallClockTimeSourceTest, SetPlaybackRate) {
  time_source_.StartTicking();

  time_source_.SetPlaybackRate(0.5);
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(0));
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(5, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(5));

  time_source_.SetPlaybackRate(2);
  EXPECT_EQ(5, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(5));
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(25, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(25));
}

TEST_F(WallClockTimeSourceTest, StopTicking) {
  time_source_.StartTicking();

  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(0));
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(10));

  time_source_.StopTicking();

  AdvanceTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNullForMediaTimeInSeconds(10));
}

}  // namespace media
