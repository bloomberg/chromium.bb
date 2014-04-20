// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/hybrid_slow_start.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class HybridSlowStartTest : public ::testing::Test {
 protected:
  HybridSlowStartTest()
     : one_ms_(QuicTime::Delta::FromMilliseconds(1)),
       rtt_(QuicTime::Delta::FromMilliseconds(60)) {
  }
  virtual void SetUp() {
    slowStart_.reset(new HybridSlowStart(&clock_));
  }
  const QuicTime::Delta one_ms_;
  const QuicTime::Delta rtt_;
  MockClock clock_;
  scoped_ptr<HybridSlowStart> slowStart_;
};

TEST_F(HybridSlowStartTest, Simple) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicPacketSequenceNumber end_sequence_number = 3;
  slowStart_->StartReceiveRound(end_sequence_number);

  EXPECT_FALSE(slowStart_->IsEndOfRound(sequence_number++));

  // Test duplicates.
  EXPECT_FALSE(slowStart_->IsEndOfRound(sequence_number));

  EXPECT_FALSE(slowStart_->IsEndOfRound(sequence_number++));
  EXPECT_TRUE(slowStart_->IsEndOfRound(sequence_number++));

  // Test without a new registered end_sequence_number;
  EXPECT_TRUE(slowStart_->IsEndOfRound(sequence_number++));

  end_sequence_number = 20;
  slowStart_->StartReceiveRound(end_sequence_number);
  while (sequence_number < end_sequence_number) {
    EXPECT_FALSE(slowStart_->IsEndOfRound(sequence_number++));
  }
  EXPECT_TRUE(slowStart_->IsEndOfRound(sequence_number++));
}

// TODO(ianswett): Add tests which more realistically invoke the methods,
// simulating how actual acks arrive and packets are sent.
TEST_F(HybridSlowStartTest, AckTrain) {
  // At a typical RTT 60 ms, assuming that the inter arrival is 1 ms,
  // we expect to be able to send a burst of 30 packet before we trigger the
  // ack train detection.
  const int kMaxLoopCount = 5;
  QuicPacketSequenceNumber sequence_number = 2;
  QuicPacketSequenceNumber end_sequence_number = 2;
  for (int burst = 0; burst < kMaxLoopCount; ++burst) {
    slowStart_->StartReceiveRound(end_sequence_number);
    do {
      clock_.AdvanceTime(one_ms_);
      EXPECT_FALSE(slowStart_->ShouldExitSlowStart(rtt_, rtt_, 100));
    }  while (!slowStart_->IsEndOfRound(sequence_number++));
    end_sequence_number *= 2;  // Exponential growth.
  }
  slowStart_->StartReceiveRound(end_sequence_number);

  for (int n = 0; n < 29 && !slowStart_->IsEndOfRound(sequence_number++); ++n) {
    clock_.AdvanceTime(one_ms_);
    EXPECT_FALSE(slowStart_->ShouldExitSlowStart(rtt_, rtt_, 100));
  }
  clock_.AdvanceTime(one_ms_);
  EXPECT_TRUE(slowStart_->ShouldExitSlowStart(rtt_, rtt_, 100));
}

TEST_F(HybridSlowStartTest, Delay) {
  // We expect to detect the increase at +1/16 of the RTT; hence at a typical
  // RTT of 60ms the detection will happen at 63.75 ms.
  const int kHybridStartMinSamples = 8;  // Number of acks required to trigger.

  QuicPacketSequenceNumber end_sequence_number = 1;
  slowStart_->StartReceiveRound(end_sequence_number++);

  // Will not trigger since our lowest RTT in our burst is the same as the long
  // term RTT provided.
  for (int n = 0; n < kHybridStartMinSamples; ++n) {
    EXPECT_FALSE(slowStart_->ShouldExitSlowStart(
        rtt_.Add(QuicTime::Delta::FromMilliseconds(n)), rtt_, 100));
  }
  slowStart_->StartReceiveRound(end_sequence_number++);
  for (int n = 1; n < kHybridStartMinSamples; ++n) {
    EXPECT_FALSE(slowStart_->ShouldExitSlowStart(
        rtt_.Add(QuicTime::Delta::FromMilliseconds(n + 5)), rtt_, 100));
  }
  // Expect to trigger since all packets in this burst was above the long term
  // RTT provided.
  EXPECT_TRUE(slowStart_->ShouldExitSlowStart(
      rtt_.Add(QuicTime::Delta::FromMilliseconds(5)), rtt_, 100));
}

}  // namespace test
}  // namespace net
