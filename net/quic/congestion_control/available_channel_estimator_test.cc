// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/available_channel_estimator.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class AvailableChannelEstimatorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    srand(1234);
    packet_size_ = 1200;
    sequence_number_ = 1;
    QuicTime receive_time = receive_clock_.Now();
    QuicTime sent_time = send_clock_.Now();
    estimator_.reset(new AvailableChannelEstimator(sequence_number_,
                                                   sent_time,
                                                   receive_time));
  }

  MockClock send_clock_;
  MockClock receive_clock_;
  QuicPacketSequenceNumber sequence_number_;
  QuicByteCount packet_size_;
  scoped_ptr<AvailableChannelEstimator> estimator_;
};

TEST_F(AvailableChannelEstimatorTest, SimpleBasic) {
  QuicBandwidth bandwidth = QuicBandwidth::Zero();
  QuicTime::Delta received_delta = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(1);
  receive_clock_.AdvanceTime(received_delta);
  send_clock_.AdvanceTime(send_delta);
  QuicTime receive_time = receive_clock_.Now();
  QuicTime sent_time = send_clock_.Now();
  estimator_->OnIncomingFeedback(++sequence_number_,
                                 packet_size_,
                                 sent_time,
                                 receive_time);
  EXPECT_EQ(kAvailableChannelEstimateUnknown,
            estimator_->GetAvailableChannelEstimate(&bandwidth));

  receive_clock_.AdvanceTime(received_delta);
  receive_time = receive_clock_.Now();
  send_clock_.AdvanceTime(send_delta);
  sent_time = send_clock_.Now();

  estimator_->OnIncomingFeedback(++sequence_number_,
                                 packet_size_,
                                 sent_time,
                                 receive_time);
  EXPECT_EQ(kAvailableChannelEstimateUncertain,
            estimator_->GetAvailableChannelEstimate(&bandwidth));

  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_, received_delta),
            bandwidth);
}

// TODO(pwestin): simulate cross traffic.
TEST_F(AvailableChannelEstimatorTest, SimpleUncertainEstimate) {
  QuicTime::Delta received_delta = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(1);

  for (int i = 0; i < 8; ++i) {
    receive_clock_.AdvanceTime(received_delta);
    QuicTime receive_time = receive_clock_.Now();
    send_clock_.AdvanceTime(send_delta);
    QuicTime sent_time = send_clock_.Now();
    estimator_->OnIncomingFeedback(++sequence_number_,
                                   packet_size_,
                                   sent_time,
                                   receive_time);
  }
  QuicBandwidth bandwidth = QuicBandwidth::Zero();
  EXPECT_EQ(kAvailableChannelEstimateUncertain,
            estimator_->GetAvailableChannelEstimate(&bandwidth));
  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_, received_delta),
            bandwidth);
}

TEST_F(AvailableChannelEstimatorTest, SimpleGoodEstimate) {
  QuicTime::Delta received_delta = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(1);

  for (int i = 0; i < 100; ++i) {
    receive_clock_.AdvanceTime(received_delta);
    QuicTime receive_time = receive_clock_.Now();
    send_clock_.AdvanceTime(send_delta);
    QuicTime sent_time = send_clock_.Now();
    estimator_->OnIncomingFeedback(++sequence_number_,
                                   packet_size_,
                                   sent_time,
                                   receive_time);
  }
  QuicBandwidth bandwidth = QuicBandwidth::Zero();
  EXPECT_EQ(kAvailableChannelEstimateGood,
            estimator_->GetAvailableChannelEstimate(&bandwidth));
  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_, received_delta),
            bandwidth);
}

}  // namespace test
}  // namespace net
