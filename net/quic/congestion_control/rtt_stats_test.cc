// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/rtt_stats.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class RttStatsTest : public ::testing::Test {
 protected:
  RttStats rtt_stats_;
};

TEST_F(RttStatsTest, MinRtt) {
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100), rtt_stats_.min_rtt());
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(10),
                       QuicTime::Delta::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.min_rtt());
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                       QuicTime::Delta::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.min_rtt());
}

TEST_F(RttStatsTest, RecentMinRtt) {
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(10),
                       QuicTime::Delta::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.min_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.recent_min_rtt());

  rtt_stats_.SampleNewRecentMinRtt(4);
  for (int i = 0; i < 3; ++i) {
    rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                       QuicTime::Delta::Zero());
    EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.min_rtt());
    EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
              rtt_stats_.recent_min_rtt());
  }
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                        QuicTime::Delta::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.min_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(50), rtt_stats_.recent_min_rtt());
}

}  // namespace test
}  // namespace net
