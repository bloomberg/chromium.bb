// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/quic/congestion_control/channel_estimator.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class ChannelEstimatorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    srand(1234);
    packet_size_ = 1200;
    sequence_number_ = 1;
  }

  QuicPacketSequenceNumber sequence_number_;
  QuicByteCount packet_size_;
  MockClock send_clock_;
  MockClock receive_clock_;
  ChannelEstimator channel_estimator_;
};

TEST_F(ChannelEstimatorTest, SimpleNonDetect) {
  // In this test, the send times differ by the same delta as the receive times,
  // so we haven't sent packets closely enough to detect "spreading," or
  // effective bandwidth.
  QuicTime::Delta delta = QuicTime::Delta::FromMilliseconds(10);

  for (int i = 0; i < 1000; ++i) {
    QuicTime send_time = send_clock_.ApproximateNow();
    QuicTime receive_time = receive_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    send_clock_.AdvanceTime(delta);
    receive_clock_.AdvanceTime(delta);
  }
  QuicBandwidth estimate = QuicBandwidth::Zero();
  EXPECT_EQ(kChannelEstimateUnknown,
            channel_estimator_.GetChannelEstimate(&estimate));
  EXPECT_TRUE(estimate.IsZero());
}

TEST_F(ChannelEstimatorTest, SimplePacketPairDetect) {
  // In this test, we start by sending packet pairs back-to-back and
  // add a receive side spreading that indicate an effective bandwidth.
  // We do 2 testes with different effective bandwidth to make sure that we
  // detect the new effective bandwidth.
  QuicTime::Delta received_delta = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(10);

  for (int i = 0; i < 100; ++i) {
    receive_clock_.AdvanceTime(received_delta);
    QuicTime receive_time = receive_clock_.ApproximateNow();
    QuicTime send_time = send_clock_.ApproximateNow();

    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    receive_clock_.AdvanceTime(received_delta);
    receive_time = receive_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    send_clock_.AdvanceTime(send_delta);
  }
  QuicBandwidth estimate = QuicBandwidth::Zero();
  EXPECT_EQ(kChannelEstimateGood,
            channel_estimator_.GetChannelEstimate(&estimate));
  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_, received_delta),
            estimate);
  received_delta = QuicTime::Delta::FromMilliseconds(1);
  for (int i = 0; i < 100; ++i) {
    receive_clock_.AdvanceTime(received_delta);
    QuicTime receive_time = receive_clock_.ApproximateNow();
    QuicTime send_time = send_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    receive_clock_.AdvanceTime(received_delta);
    receive_time = receive_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    send_clock_.AdvanceTime(send_delta);
  }
  EXPECT_EQ(kChannelEstimateGood,
            channel_estimator_.GetChannelEstimate(&estimate));
  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_, received_delta),
            estimate);
}

TEST_F(ChannelEstimatorTest, SimpleFlatSlope) {
  // In this test, we send packet pairs back-to-back and add a slowly increasing
  // receive side spreading. We expect the estimate to be good and that our
  // mean receive side spreading is returned as the estimate.
  QuicTime::Delta initial_received_delta = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta received_delta = initial_received_delta;
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(10);

  for (int i = 0; i < 100; ++i) {
    receive_clock_.AdvanceTime(received_delta);
    QuicTime receive_time = receive_clock_.ApproximateNow();
    QuicTime send_time = send_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    receive_clock_.AdvanceTime(received_delta);
    receive_time = receive_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    send_clock_.AdvanceTime(send_delta);
    received_delta = received_delta.Add(QuicTime::Delta::FromMicroseconds(10));
  }
  QuicBandwidth estimate = QuicBandwidth::Zero();
  EXPECT_EQ(kChannelEstimateGood,
            channel_estimator_.GetChannelEstimate(&estimate));

  // Calculate our mean receive delta.
  QuicTime::Delta increased_received_delta =
      received_delta.Subtract(initial_received_delta);
  QuicTime::Delta mean_received_delta = initial_received_delta.Add(
      QuicTime::Delta::FromMicroseconds(
          increased_received_delta.ToMicroseconds() / 2));

  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_,
      mean_received_delta), estimate);
}

TEST_F(ChannelEstimatorTest, SimpleMediumSlope) {
  // In this test, we send packet pairs back-to-back and add an increasing
  // receive side spreading. We expect the estimate to be uncertaint and that
  // our mean receive side spreading is returned as the estimate.
  QuicTime::Delta initial_received_delta = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta received_delta = initial_received_delta;
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(10);

  for (int i = 0; i < 100; ++i) {
    receive_clock_.AdvanceTime(received_delta);
    QuicTime receive_time = receive_clock_.ApproximateNow();
    QuicTime send_time = send_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    receive_clock_.AdvanceTime(received_delta);
    receive_time = receive_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    send_clock_.AdvanceTime(send_delta);
    received_delta = received_delta.Add(QuicTime::Delta::FromMicroseconds(50));
  }
  QuicBandwidth estimate = QuicBandwidth::Zero();
  EXPECT_EQ(kChannelEstimateUncertain,
            channel_estimator_.GetChannelEstimate(&estimate));

  // Calculate our mean receive delta.
  QuicTime::Delta increased_received_delta =
      received_delta.Subtract(initial_received_delta);
  QuicTime::Delta mean_received_delta = initial_received_delta.Add(
      QuicTime::Delta::FromMicroseconds(
          increased_received_delta.ToMicroseconds() / 2));

  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_,
      mean_received_delta), estimate);
}

TEST_F(ChannelEstimatorTest, SimpleSteepSlope) {
  // In this test, we send packet pairs back-to-back and add a rapidly
  // increasing receive side spreading. We expect the estimate to be uncertain
  // and that our mean receive side spreading is returned as the estimate.
  QuicTime::Delta initial_received_delta = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta received_delta = initial_received_delta;
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(10);

  for (int i = 0; i < 100; ++i) {
    receive_clock_.AdvanceTime(received_delta);
    QuicTime receive_time = receive_clock_.ApproximateNow();
    QuicTime send_time = send_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    receive_clock_.AdvanceTime(received_delta);
    receive_time = receive_clock_.ApproximateNow();
    channel_estimator_.OnAcknowledgedPacket(sequence_number_++,
                                            packet_size_,
                                            send_time,
                                            receive_time);
    send_clock_.AdvanceTime(send_delta);
    received_delta = received_delta.Add(QuicTime::Delta::FromMicroseconds(100));
  }
  QuicBandwidth estimate = QuicBandwidth::Zero();
  EXPECT_EQ(kChannelEstimateUncertain,
            channel_estimator_.GetChannelEstimate(&estimate));

  // Calculate our mean receive delta.
  QuicTime::Delta increased_received_delta =
      received_delta.Subtract(initial_received_delta);
  QuicTime::Delta mean_received_delta = initial_received_delta.Add(
      QuicTime::Delta::FromMicroseconds(
          increased_received_delta.ToMicroseconds() / 2));

  EXPECT_EQ(QuicBandwidth::FromBytesAndTimeDelta(packet_size_,
      mean_received_delta), estimate);
}

}  // namespace test
}  // namespace net
