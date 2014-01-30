// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/congestion_control/congestion_control.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const uint32 kMaxBitrateConfigured = 5000000;
static const uint32 kMinBitrateConfigured = 500000;
static const uint32 kStartBitrate = 2000000;
static const int64 kStartMillisecond = GG_INT64_C(12345678900000);
static const int64 kRttMs = 20;
static const int64 kAckRateMs = 33;

class CongestionControlTest : public ::testing::Test {
 protected:
  CongestionControlTest()
      : congestion_control_(&testing_clock_,
                            kDefaultCongestionControlBackOff,
                            kMaxBitrateConfigured,
                            kMinBitrateConfigured,
                            kStartBitrate) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  // Returns the last bitrate of the run.
  uint32 RunWithOneLossEventPerSecond(int fps,
                                      int rtt_ms,
                                      int runtime_in_seconds) {
    const base::TimeDelta rtt = base::TimeDelta::FromMilliseconds(rtt_ms);
    const base::TimeDelta ack_rate =
        base::TimeDelta::FromMilliseconds(GG_INT64_C(1000) / fps);
    uint32 new_bitrate = 0;
    EXPECT_FALSE(congestion_control_.OnAck(rtt, &new_bitrate));

    for (int seconds = 0; seconds < runtime_in_seconds; ++seconds) {
      for (int i = 1; i < fps; ++i) {
        testing_clock_.Advance(ack_rate);
        congestion_control_.OnAck(rtt, &new_bitrate);
      }
      EXPECT_TRUE(congestion_control_.OnNack(rtt, &new_bitrate));
    }
    return new_bitrate;
  }

  base::SimpleTestTickClock testing_clock_;
  CongestionControl congestion_control_;

  DISALLOW_COPY_AND_ASSIGN(CongestionControlTest);
};

TEST_F(CongestionControlTest, Max) {
  uint32 new_bitrate = 0;
  const base::TimeDelta rtt = base::TimeDelta::FromMilliseconds(kRttMs);
  const base::TimeDelta ack_rate =
      base::TimeDelta::FromMilliseconds(kAckRateMs);
  EXPECT_FALSE(congestion_control_.OnAck(rtt, &new_bitrate));

  uint32 expected_increase_bitrate = 0;

  // Expected time is 5 seconds. 500000 - 2000000 = 5 * 1500 * 8 * (1000 / 20).
  for (int i = 0; i < 151; ++i) {
    testing_clock_.Advance(ack_rate);
    EXPECT_TRUE(congestion_control_.OnAck(rtt, &new_bitrate));
    expected_increase_bitrate += 1500 * 8 * kAckRateMs / kRttMs;
    EXPECT_EQ(kStartBitrate + expected_increase_bitrate, new_bitrate);
  }
  testing_clock_.Advance(ack_rate);
  EXPECT_TRUE(congestion_control_.OnAck(rtt, &new_bitrate));
  EXPECT_EQ(kMaxBitrateConfigured, new_bitrate);
}

TEST_F(CongestionControlTest, Min) {
  uint32 new_bitrate = 0;
  const base::TimeDelta rtt = base::TimeDelta::FromMilliseconds(kRttMs);
  const base::TimeDelta ack_rate =
      base::TimeDelta::FromMilliseconds(kAckRateMs);
  EXPECT_FALSE(congestion_control_.OnNack(rtt, &new_bitrate));

  uint32 expected_decrease_bitrate = kStartBitrate;

  // Expected number is 10. 2000 * 0.875^10 <= 500.
  for (int i = 0; i < 10; ++i) {
    testing_clock_.Advance(ack_rate);
    EXPECT_TRUE(congestion_control_.OnNack(rtt, &new_bitrate));
    expected_decrease_bitrate = static_cast<uint32>(
        expected_decrease_bitrate * kDefaultCongestionControlBackOff);
    EXPECT_EQ(expected_decrease_bitrate, new_bitrate);
  }
  testing_clock_.Advance(ack_rate);
  EXPECT_TRUE(congestion_control_.OnNack(rtt, &new_bitrate));
  EXPECT_EQ(kMinBitrateConfigured, new_bitrate);
}

TEST_F(CongestionControlTest, Timing) {
  const base::TimeDelta rtt = base::TimeDelta::FromMilliseconds(kRttMs);
  const base::TimeDelta ack_rate =
      base::TimeDelta::FromMilliseconds(kAckRateMs);
  uint32 new_bitrate = 0;
  uint32 expected_bitrate = kStartBitrate;

  EXPECT_FALSE(congestion_control_.OnAck(rtt, &new_bitrate));

  testing_clock_.Advance(ack_rate);
  EXPECT_TRUE(congestion_control_.OnAck(rtt, &new_bitrate));
  expected_bitrate += 1500 * 8 * kAckRateMs / kRttMs;
  EXPECT_EQ(expected_bitrate, new_bitrate);

  // We should back immediately.
  EXPECT_TRUE(congestion_control_.OnNack(rtt, &new_bitrate));
  expected_bitrate =
      static_cast<uint32>(expected_bitrate * kDefaultCongestionControlBackOff);
  EXPECT_EQ(expected_bitrate, new_bitrate);

  // Less than one RTT have passed don't back again.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(congestion_control_.OnNack(rtt, &new_bitrate));

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(congestion_control_.OnNack(rtt, &new_bitrate));
  expected_bitrate =
      static_cast<uint32>(expected_bitrate * kDefaultCongestionControlBackOff);
  EXPECT_EQ(expected_bitrate, new_bitrate);

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(congestion_control_.OnAck(rtt, &new_bitrate));
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(congestion_control_.OnAck(rtt, &new_bitrate));
  expected_bitrate += 1500 * 8 * 20 / kRttMs;
  EXPECT_EQ(expected_bitrate, new_bitrate);

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(congestion_control_.OnAck(rtt, &new_bitrate));
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(congestion_control_.OnAck(rtt, &new_bitrate));
  expected_bitrate += 1500 * 8 * 20 / kRttMs;
  EXPECT_EQ(expected_bitrate, new_bitrate);

  // Test long elapsed time (300 ms).
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(300));
  EXPECT_TRUE(congestion_control_.OnAck(rtt, &new_bitrate));
  expected_bitrate += 1500 * 8 * 100 / kRttMs;
  EXPECT_EQ(expected_bitrate, new_bitrate);

  // Test many short elapsed time (1 ms).
  for (int i = 0; i < 19; ++i) {
    testing_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
    EXPECT_FALSE(congestion_control_.OnAck(rtt, &new_bitrate));
  }
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(congestion_control_.OnAck(rtt, &new_bitrate));
  expected_bitrate += 1500 * 8 * 20 / kRttMs;
  EXPECT_EQ(expected_bitrate, new_bitrate);
}

TEST_F(CongestionControlTest, Convergence24fps) {
  EXPECT_GE(RunWithOneLossEventPerSecond(24, kRttMs, 100),
            GG_UINT32_C(3000000));
}

TEST_F(CongestionControlTest, Convergence24fpsLongRtt) {
  EXPECT_GE(RunWithOneLossEventPerSecond(24, 100, 100), GG_UINT32_C(500000));
}

TEST_F(CongestionControlTest, Convergence60fps) {
  EXPECT_GE(RunWithOneLossEventPerSecond(60, kRttMs, 100),
            GG_UINT32_C(3500000));
}

TEST_F(CongestionControlTest, Convergence60fpsLongRtt) {
  EXPECT_GE(RunWithOneLossEventPerSecond(60, 100, 100), GG_UINT32_C(500000));
}

}  // namespace cast
}  // namespace media
