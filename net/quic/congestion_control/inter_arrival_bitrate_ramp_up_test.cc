// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/quic/congestion_control/inter_arrival_bitrate_ramp_up.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class InterArrivalBitrateRampUpTest : public ::testing::Test {
 protected:
  InterArrivalBitrateRampUpTest()
      : one_ms_(QuicTime::Delta::FromMilliseconds(1)),
        hundred_ms_(QuicTime::Delta::FromMilliseconds(100)),
        bitrate_ramp_up_(&clock_) {
  }
  virtual void SetUp() {
    clock_.AdvanceTime(one_ms_);
  }
  const QuicTime::Delta one_ms_;
  const QuicTime::Delta hundred_ms_;
  MockClock clock_;
  InterArrivalBitrateRampUp bitrate_ramp_up_;
};

TEST_F(InterArrivalBitrateRampUpTest, GoodEstimates) {
  QuicBandwidth start_rate = QuicBandwidth::FromKBytesPerSecond(100);
  QuicBandwidth available_channel_estimate =
      QuicBandwidth::FromKBytesPerSecond(200);
  QuicBandwidth channel_estimate = QuicBandwidth::FromKBytesPerSecond(400);
  QuicBandwidth halfway_point = available_channel_estimate.Add(
      channel_estimate.Subtract(available_channel_estimate).Scale(0.5f));
  QuicBandwidth sent_bitrate = QuicBandwidth::Zero();
  bitrate_ramp_up_.Reset(start_rate,
                         available_channel_estimate,
                         channel_estimate);

  // First concave growth, towards available_channel_estimate.
  for (int i = 0; i < 25; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(available_channel_estimate, sent_bitrate);
    EXPECT_LE(start_rate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(available_channel_estimate, sent_bitrate);

  // First convex growth, from available_channel_estimate.
  for (int j = 0; j < 25; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(available_channel_estimate, sent_bitrate);
    EXPECT_GE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(halfway_point, sent_bitrate);

  // Second concave growth, towards channel_estimate.
  for (int i = 0; i < 24; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(channel_estimate, sent_bitrate);
    EXPECT_LE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(channel_estimate, sent_bitrate);

  // Second convex growth, from channel_estimate.
  for (int j = 0; j < 25; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(channel_estimate.ToBytesPerSecond() + 100000,
              sent_bitrate.ToBytesPerSecond(), 10000);

  // Verify that we increase cubic.
  for (int j = 0; j < 23; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(channel_estimate.ToBytesPerSecond() + 750000,
              sent_bitrate.ToBytesPerSecond(), 10000);
}

TEST_F(InterArrivalBitrateRampUpTest, GoodEstimatesLimitedSendRate) {
  QuicBandwidth start_rate = QuicBandwidth::FromKBytesPerSecond(100);
  QuicBandwidth available_channel_estimate =
      QuicBandwidth::FromKBytesPerSecond(200);
  QuicBandwidth max_sent_rate =
      QuicBandwidth::FromKBytesPerSecond(125);
  QuicBandwidth channel_estimate = QuicBandwidth::FromKBytesPerSecond(400);
  QuicBandwidth halfway_point = available_channel_estimate.Add(
      channel_estimate.Subtract(available_channel_estimate).Scale(0.5f));
  QuicBandwidth sent_bitrate = QuicBandwidth::Zero();
  bitrate_ramp_up_.Reset(start_rate,
                         available_channel_estimate,
                         channel_estimate);

  // First concave growth, towards available_channel_estimate.
  // Should pass without being affected by the max_sent_rate.
  for (int i = 0; i < 25; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    // Cap our previus sent rate.
    sent_bitrate = std::min(sent_bitrate, max_sent_rate);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(available_channel_estimate, sent_bitrate);
    EXPECT_LE(start_rate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  // Cap our previus sent rate.
  sent_bitrate = std::min(sent_bitrate, max_sent_rate);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(available_channel_estimate, sent_bitrate);

  // First convex growth, from available_channel_estimate.
  for (int j = 0; j < 25; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    // Cap our previus sent rate.
    sent_bitrate = std::min(sent_bitrate, max_sent_rate);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(available_channel_estimate, sent_bitrate);
    EXPECT_GE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = std::min(sent_bitrate, max_sent_rate);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  // We expect 2 * sent_bitrate to cap the rate.
  EXPECT_LE(max_sent_rate.Add(max_sent_rate), sent_bitrate);
  // Remove our sent cap.
  // Expect bitrate to continue to ramp from its previous rate.
  for (int j = 0; j < 5; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(available_channel_estimate, sent_bitrate);
    EXPECT_LE(max_sent_rate.Add(max_sent_rate), sent_bitrate);
    EXPECT_GE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(halfway_point, sent_bitrate);
}

TEST_F(InterArrivalBitrateRampUpTest, GoodEstimatesCloseToChannelEstimate) {
  QuicBandwidth start_rate = QuicBandwidth::FromKBytesPerSecond(100);
  QuicBandwidth available_channel_estimate =
      QuicBandwidth::FromKBytesPerSecond(200);
  QuicBandwidth channel_estimate = QuicBandwidth::FromKBytesPerSecond(250);
  QuicBandwidth halfway_point = available_channel_estimate.Add(
      channel_estimate.Subtract(available_channel_estimate).Scale(0.5f));
  QuicBandwidth sent_bitrate = QuicBandwidth::Zero();
  bitrate_ramp_up_.Reset(start_rate,
                         available_channel_estimate,
                         channel_estimate);

  // First concave growth, towards available_channel_estimate.
  for (int i = 0; i < 25; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(available_channel_estimate, sent_bitrate);
    EXPECT_LE(start_rate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(available_channel_estimate, sent_bitrate);

  // First convex growth, from available_channel_estimate.
  for (int j = 0; j < 15; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(available_channel_estimate, sent_bitrate);
    EXPECT_GE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(halfway_point, sent_bitrate);

  // Second concave growth, towards channel_estimate.
  for (int i = 0; i < 14; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(channel_estimate, sent_bitrate);
    EXPECT_LE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(channel_estimate, sent_bitrate);

  // Second convex growth, from channel_estimate.
  for (int j = 0; j < 25; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(channel_estimate.ToBytesPerSecond() + 100000,
              sent_bitrate.ToBytesPerSecond(), 10000);

  // Verify that we increase cubic.
  for (int j = 0; j < 24; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(channel_estimate.ToBytesPerSecond() + 780000,
              sent_bitrate.ToBytesPerSecond(), 20000);
}

TEST_F(InterArrivalBitrateRampUpTest, UncertainEstimates) {
  QuicBandwidth start_rate = QuicBandwidth::FromKBytesPerSecond(100);
  QuicBandwidth available_channel_estimate =
      QuicBandwidth::FromKBytesPerSecond(200);
  QuicBandwidth channel_estimate =
      QuicBandwidth::FromKBytesPerSecond(400 * 0.7f);
  QuicBandwidth halfway_point = available_channel_estimate.Add(
      channel_estimate.Subtract(available_channel_estimate).Scale(0.5f));
  QuicBandwidth sent_bitrate = QuicBandwidth::Zero();
  bitrate_ramp_up_.Reset(start_rate,
                         available_channel_estimate,
                         channel_estimate);

  // First concave growth, towards available_channel_estimate.
  for (int i = 0; i < 20; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(available_channel_estimate, sent_bitrate);
    EXPECT_LE(start_rate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(available_channel_estimate, sent_bitrate);

  // First convex growth, from available_channel_estimate.
  for (int j = 0; j < 23; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(available_channel_estimate, sent_bitrate);
    EXPECT_GE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(halfway_point, sent_bitrate);

  // Second concave growth, towards channel_estimate.
  for (int i = 0; i < 12; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(channel_estimate, sent_bitrate);
    EXPECT_LE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(channel_estimate, sent_bitrate);

  // Second convex growth, from channel_estimate.
  for (int j = 0; j < 30; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(channel_estimate.ToBytesPerSecond() + 100000,
              sent_bitrate.ToBytesPerSecond(), 10000);

  // Verify that we increase cubic.
  for (int j = 0; j < 23; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(channel_estimate.ToBytesPerSecond() + 750000,
              sent_bitrate.ToBytesPerSecond(), 20000);
}

TEST_F(InterArrivalBitrateRampUpTest, UnknownEstimates) {
  QuicBandwidth start_rate = QuicBandwidth::FromKBytesPerSecond(100);
  QuicBandwidth available_channel_estimate =
      QuicBandwidth::FromKBytesPerSecond(200);
  QuicBandwidth channel_estimate = QuicBandwidth::FromKBytesPerSecond(400);
  QuicBandwidth sent_bitrate = QuicBandwidth::Zero();
  bitrate_ramp_up_.Reset(start_rate,
                         available_channel_estimate,
                         available_channel_estimate);

  // First convex growth, from start_rate.
  for (int j = 0; j < 20; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(start_rate, sent_bitrate);
    EXPECT_GE(available_channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(available_channel_estimate.ToBytesPerSecond(),
              sent_bitrate.ToBytesPerSecond(), 10000);

  // Verify that we increase cubic.
  for (int j = 0; j < 31; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(channel_estimate, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_NEAR(channel_estimate.ToBytesPerSecond(),
              sent_bitrate.ToBytesPerSecond(), 10000);
}

TEST_F(InterArrivalBitrateRampUpTest, UpdatingChannelEstimateHigher) {
  QuicBandwidth start_rate = QuicBandwidth::FromKBytesPerSecond(200);
  QuicBandwidth available_channel_estimate = start_rate;
  QuicBandwidth channel_estimate = QuicBandwidth::FromKBytesPerSecond(250);
  QuicBandwidth halfway_point = available_channel_estimate.Add(
      channel_estimate.Subtract(available_channel_estimate).Scale(0.5f));
  QuicBandwidth sent_bitrate = QuicBandwidth::Zero();
  bitrate_ramp_up_.Reset(start_rate,
                         available_channel_estimate,
                         channel_estimate);

  // Convex growth, from available_channel_estimate.
  for (int j = 0; j < 16; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(available_channel_estimate, sent_bitrate);
    EXPECT_GE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(halfway_point, sent_bitrate);

  // Increse channel estimate.
  channel_estimate = QuicBandwidth::FromKBytesPerSecond(300);
  bitrate_ramp_up_.UpdateChannelEstimate(channel_estimate);

  // Concave growth, towards channel_estimate.
  for (int i = 0; i < 22; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(channel_estimate, sent_bitrate);
    EXPECT_LE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(channel_estimate, sent_bitrate);
}

TEST_F(InterArrivalBitrateRampUpTest, UpdatingChannelEstimateLower) {
  QuicBandwidth start_rate = QuicBandwidth::FromKBytesPerSecond(200);
  QuicBandwidth available_channel_estimate = start_rate;
  QuicBandwidth channel_estimate = QuicBandwidth::FromKBytesPerSecond(250);
  QuicBandwidth halfway_point = available_channel_estimate.Add(
      channel_estimate.Subtract(available_channel_estimate).Scale(0.5f));
  QuicBandwidth sent_bitrate = QuicBandwidth::Zero();
  bitrate_ramp_up_.Reset(start_rate,
                         available_channel_estimate,
                         channel_estimate);

  // Convex growth, from available_channel_estimate.
  for (int j = 0; j < 16; ++j) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_LE(available_channel_estimate, sent_bitrate);
    EXPECT_GE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(halfway_point, sent_bitrate);

  // Decrese channel estimate.
  channel_estimate = QuicBandwidth::FromKBytesPerSecond(240);
  bitrate_ramp_up_.UpdateChannelEstimate(channel_estimate);

  // Concave growth, towards channel_estimate.
  for (int i = 0; i < 11; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
    EXPECT_GE(channel_estimate, sent_bitrate);
    EXPECT_LE(halfway_point, sent_bitrate);
  }
  clock_.AdvanceTime(hundred_ms_);
  sent_bitrate = bitrate_ramp_up_.GetNewBitrate(sent_bitrate);
  EXPECT_LE(channel_estimate, sent_bitrate);
}

}  // namespace test
}  // namespace net
