// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_time.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class QuicTimeDeltaTest : public ::testing::Test {
 protected:
};

TEST_F(QuicTimeDeltaTest, Zero) {
  EXPECT_TRUE(QuicTime::Delta::Zero().IsZero());
  EXPECT_FALSE(QuicTime::Delta::Zero().IsInfinite());
  EXPECT_FALSE(QuicTime::Delta::FromMilliseconds(1).IsZero());
}

TEST_F(QuicTimeDeltaTest, Infinite) {
  EXPECT_TRUE(QuicTime::Delta::Infinite().IsInfinite());
  EXPECT_FALSE(QuicTime::Delta::Zero().IsInfinite());
  EXPECT_FALSE(QuicTime::Delta::FromMilliseconds(1).IsInfinite());
}

TEST_F(QuicTimeDeltaTest, FromTo) {
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(1),
            QuicTime::Delta::FromMicroseconds(1000));
  EXPECT_EQ(QuicTime::Delta::FromSeconds(1),
            QuicTime::Delta::FromMilliseconds(1000));
  EXPECT_EQ(QuicTime::Delta::FromSeconds(1),
            QuicTime::Delta::FromMicroseconds(1000000));

  EXPECT_EQ(1, QuicTime::Delta::FromMicroseconds(1000).ToMilliseconds());
  EXPECT_EQ(2, QuicTime::Delta::FromMilliseconds(2000).ToSeconds());
  EXPECT_EQ(1000, QuicTime::Delta::FromMilliseconds(1).ToMicroseconds());
  EXPECT_EQ(1, QuicTime::Delta::FromMicroseconds(1000).ToMilliseconds());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(2000).ToMicroseconds(),
            QuicTime::Delta::FromSeconds(2).ToMicroseconds());
}

TEST_F(QuicTimeDeltaTest, Add) {
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2000),
            QuicTime::Delta::Zero().Add(QuicTime::Delta::FromMilliseconds(2)));
}

TEST_F(QuicTimeDeltaTest, Subtract) {
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1000),
            QuicTime::Delta::FromMilliseconds(2).Subtract(
                QuicTime::Delta::FromMilliseconds(1)));
}

class QuicTimeTest : public ::testing::Test {
 protected:
  MockClock clock_;
};

TEST_F(QuicTimeTest, Initialized) {
  EXPECT_FALSE(QuicTime::Zero().IsInitialized());
  EXPECT_TRUE(QuicTime::Zero().Add(
      QuicTime::Delta::FromMicroseconds(1)).IsInitialized());
}

TEST_F(QuicTimeTest, Add) {
  QuicTime time_1 = QuicTime::Zero().Add(
      QuicTime::Delta::FromMilliseconds(1));
  QuicTime time_2 = QuicTime::Zero().Add(
      QuicTime::Delta::FromMilliseconds(2));

  QuicTime::Delta diff = time_2.Subtract(time_1);

  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(1), diff);
  EXPECT_EQ(1000, diff.ToMicroseconds());
  EXPECT_EQ(1, diff.ToMilliseconds());
}

TEST_F(QuicTimeTest, Subtract) {
  QuicTime time_1 = QuicTime::Zero().Add(
      QuicTime::Delta::FromMilliseconds(1));
  QuicTime time_2 = QuicTime::Zero().Add(
      QuicTime::Delta::FromMilliseconds(2));

  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(1), time_2.Subtract(time_1));
}

TEST_F(QuicTimeTest, SubtractDelta) {
  QuicTime time = QuicTime::Zero().Add(
      QuicTime::Delta::FromMilliseconds(2));
  EXPECT_EQ(QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(1)),
            time.Subtract(QuicTime::Delta::FromMilliseconds(1)));
}

TEST_F(QuicTimeTest, MockClock) {
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));

  QuicTime now = clock_.ApproximateNow();
  QuicTime time = QuicTime::Zero().Add(QuicTime::Delta::FromMicroseconds(1000));

  EXPECT_EQ(now, time);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  now = clock_.ApproximateNow();

  EXPECT_NE(now, time);

  time = time.Add(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(now, time);
}

}  // namespace test
}  // namespace net
