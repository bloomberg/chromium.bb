// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "media/cast/rtp_common/rtp_defines.h"
#include "media/cast/rtp_receiver/receiver_stats.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = 123456789;
static const uint32 kStdTimeIncrementMs = 33;
static const uint32 kSsrc = 0x1234;

class ReceiverStatsTest : public ::testing::Test {
 protected:
  ReceiverStatsTest()
      : stats_(kSsrc),
        rtp_header_(),
        fraction_lost_(0),
        cumulative_lost_(0),
        extended_high_sequence_number_(0),
        jitter_(0) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    start_time_ =  testing_clock_.NowTicks();
    delta_increments_ = base::TimeDelta::FromMilliseconds(kStdTimeIncrementMs);
  }
  ~ReceiverStatsTest() {}

  virtual void SetUp() {
    rtp_header_.webrtc.header.sequenceNumber = 0;
    rtp_header_.webrtc.header.timestamp = 0;
    rtp_header_.webrtc.header.ssrc = kSsrc;
  }

  uint32 ExpectedJitter(uint32 const_interval, int num_packets) {
    float jitter = 0;
    // Assume timestamps have a constant kStdTimeIncrementMs interval.
    float float_interval =
        static_cast<float>(const_interval - kStdTimeIncrementMs);
    for (int i = 0; i < num_packets; ++i) {
      jitter += (float_interval - jitter) / 16;
    }
    return static_cast<uint32>(jitter + 0.5f);
  }

  uint32 Timestamp() {
    base::TimeDelta delta = testing_clock_.NowTicks() - start_time_;
    return static_cast<uint32>(delta.InMilliseconds() * 90);
  }

  ReceiverStats stats_;
  RtpCastHeader rtp_header_;
  uint8 fraction_lost_;
  uint32 cumulative_lost_;
  uint32 extended_high_sequence_number_;
  uint32 jitter_;
  base::SimpleTestTickClock testing_clock_;
  base::TimeTicks start_time_;
  base::TimeDelta delta_increments_;
};

TEST_F(ReceiverStatsTest, ResetState) {
  stats_.GetStatistics(&fraction_lost_, &cumulative_lost_,
      &extended_high_sequence_number_, &jitter_);
  EXPECT_EQ(0u, fraction_lost_);
  EXPECT_EQ(0u, cumulative_lost_);
  EXPECT_EQ(0u, extended_high_sequence_number_);
  EXPECT_EQ(0u, jitter_);
}

TEST_F(ReceiverStatsTest, LossCount) {
  for (int i = 0; i < 300; ++i) {
    if (i % 4)
      stats_.UpdateStatistics(rtp_header_);
    if (i % 3) {
      rtp_header_.webrtc.header.timestamp = Timestamp();
    }
    ++rtp_header_.webrtc.header.sequenceNumber;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_, &cumulative_lost_,
      &extended_high_sequence_number_, &jitter_);
  EXPECT_EQ(63u, fraction_lost_);
  EXPECT_EQ(74u, cumulative_lost_);
  // Build extended sequence number.
  uint32 extended_seq_num = rtp_header_.webrtc.header.sequenceNumber - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
}

TEST_F(ReceiverStatsTest, NoLossWrap) {
  rtp_header_.webrtc.header.sequenceNumber = 65500;
  for (int i = 0; i < 300; ++i) {
      stats_.UpdateStatistics(rtp_header_);
    if (i % 3) {
      rtp_header_.webrtc.header.timestamp = Timestamp();
    }
    ++rtp_header_.webrtc.header.sequenceNumber;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_, &cumulative_lost_,
      &extended_high_sequence_number_, &jitter_);
  EXPECT_EQ(0u, fraction_lost_);
  EXPECT_EQ(0u, cumulative_lost_);
  // Build extended sequence number (one wrap cycle).
  uint32 extended_seq_num = (1 << 16) +
      rtp_header_.webrtc.header.sequenceNumber - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
}

TEST_F(ReceiverStatsTest, LossCountWrap) {
  const uint32 start_sequence_number = 65500;
  rtp_header_.webrtc.header.sequenceNumber = start_sequence_number;
  for (int i = 0; i < 300; ++i) {
    if (i % 4)
      stats_.UpdateStatistics(rtp_header_);
    if (i % 3)
      // Update timestamp.
      ++rtp_header_.webrtc.header.timestamp;
    ++rtp_header_.webrtc.header.sequenceNumber;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_, &cumulative_lost_,
      &extended_high_sequence_number_, &jitter_);
  EXPECT_EQ(63u, fraction_lost_);
  EXPECT_EQ(74u, cumulative_lost_);
  // Build extended sequence number (one wrap cycle).
  uint32 extended_seq_num = (1 << 16) +
      rtp_header_.webrtc.header.sequenceNumber - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
}

TEST_F(ReceiverStatsTest, Jitter) {
  rtp_header_.webrtc.header.timestamp = Timestamp();
  for (int i = 0; i < 300; ++i) {
    stats_.UpdateStatistics(rtp_header_);
    ++rtp_header_.webrtc.header.sequenceNumber;
    rtp_header_.webrtc.header.timestamp += 33 * 90;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_, &cumulative_lost_,
      &extended_high_sequence_number_, &jitter_);
  EXPECT_FALSE(fraction_lost_);
  EXPECT_FALSE(cumulative_lost_);
  // Build extended sequence number (one wrap cycle).
  uint32 extended_seq_num = rtp_header_.webrtc.header.sequenceNumber - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
  EXPECT_EQ(ExpectedJitter(kStdTimeIncrementMs, 300), jitter_);
}

}  // namespace cast
}  // namespace media
