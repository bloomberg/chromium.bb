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
  void SetUp() override { slow_start_.reset(new HybridSlowStart(&clock_)); }
  const QuicTime::Delta one_ms_;
  const QuicTime::Delta rtt_;
  MockClock clock_;
  scoped_ptr<HybridSlowStart> slow_start_;
};

TEST_F(HybridSlowStartTest, Simple) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicPacketSequenceNumber end_sequence_number = 3;
  slow_start_->StartReceiveRound(end_sequence_number);

  EXPECT_FALSE(slow_start_->IsEndOfRound(sequence_number++));

  // Test duplicates.
  EXPECT_FALSE(slow_start_->IsEndOfRound(sequence_number));

  EXPECT_FALSE(slow_start_->IsEndOfRound(sequence_number++));
  EXPECT_TRUE(slow_start_->IsEndOfRound(sequence_number++));

  // Test without a new registered end_sequence_number;
  EXPECT_TRUE(slow_start_->IsEndOfRound(sequence_number++));

  end_sequence_number = 20;
  slow_start_->StartReceiveRound(end_sequence_number);
  while (sequence_number < end_sequence_number) {
    EXPECT_FALSE(slow_start_->IsEndOfRound(sequence_number++));
  }
  EXPECT_TRUE(slow_start_->IsEndOfRound(sequence_number++));
}

TEST_F(HybridSlowStartTest, Delay) {
  // We expect to detect the increase at +1/8 of the RTT; hence at a typical
  // RTT of 60ms the detection will happen at 67.5 ms.
  const int kHybridStartMinSamples = 8;  // Number of acks required to trigger.

  QuicPacketSequenceNumber end_sequence_number = 1;
  slow_start_->StartReceiveRound(end_sequence_number++);

  // Will not trigger since our lowest RTT in our burst is the same as the long
  // term RTT provided.
  for (int n = 0; n < kHybridStartMinSamples; ++n) {
    EXPECT_FALSE(slow_start_->ShouldExitSlowStart(
        rtt_.Add(QuicTime::Delta::FromMilliseconds(n)), rtt_, 100));
  }
  slow_start_->StartReceiveRound(end_sequence_number++);
  for (int n = 1; n < kHybridStartMinSamples; ++n) {
    EXPECT_FALSE(slow_start_->ShouldExitSlowStart(
        rtt_.Add(QuicTime::Delta::FromMilliseconds(n + 10)), rtt_, 100));
  }
  // Expect to trigger since all packets in this burst was above the long term
  // RTT provided.
  EXPECT_TRUE(slow_start_->ShouldExitSlowStart(
      rtt_.Add(QuicTime::Delta::FromMilliseconds(10)), rtt_, 100));
}

}  // namespace test
}  // namespace net
