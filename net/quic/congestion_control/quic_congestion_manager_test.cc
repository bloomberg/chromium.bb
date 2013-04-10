// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/quic_congestion_manager.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace net {
namespace test {

class QuicCongestionManagerPeer : public QuicCongestionManager {
 public:
  explicit QuicCongestionManagerPeer(const QuicClock* clock,
                                     CongestionFeedbackType congestion_type)
      : QuicCongestionManager(clock, congestion_type) {
  }
  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    this->send_algorithm_.reset(send_algorithm);
  }

  using QuicCongestionManager::rtt;
  using QuicCongestionManager::SentBandwidth;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCongestionManagerPeer);
};

class QuicCongestionManagerTest : public ::testing::Test {
 protected:
  void SetUpCongestionType(CongestionFeedbackType congestion_type) {
    manager_.reset(new QuicCongestionManagerPeer(&clock_, congestion_type));
  }
  MockClock clock_;
  scoped_ptr<QuicCongestionManagerPeer> manager_;
};

TEST_F(QuicCongestionManagerTest, Bandwidth) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  manager_->OnIncomingAckFrame(ack, clock_.Now());

  QuicCongestionFeedbackFrame feedback;
  feedback.type = kFixRate;
  feedback.fix_rate.bitrate = QuicBandwidth::FromKBytesPerSecond(100);
  manager_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());

  for (int i = 1; i <= 100; ++i) {
    QuicTime::Delta advance_time = manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    clock_.AdvanceTime(advance_time);
    EXPECT_TRUE(manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA).IsZero());
    manager_->SentPacket(i, clock_.Now(), 1000, NOT_RETRANSMISSION);
    // Ack the packet we sent.
    ack.received_info.largest_observed = i;
    manager_->OnIncomingAckFrame(ack, clock_.Now());
  }
  EXPECT_EQ(100, manager_->BandwidthEstimate().ToKBytesPerSecond());
  EXPECT_NEAR(100,
              manager_->SentBandwidth(clock_.Now()).ToKBytesPerSecond(), 4);
}

TEST_F(QuicCongestionManagerTest, BandwidthWith1SecondGap) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  manager_->OnIncomingAckFrame(ack, clock_.Now());

  QuicCongestionFeedbackFrame feedback;
  feedback.type = kFixRate;
  feedback.fix_rate.bitrate = QuicBandwidth::FromKBytesPerSecond(100);
  manager_->OnIncomingQuicCongestionFeedbackFrame(feedback, clock_.Now());

  for (QuicPacketSequenceNumber sequence_number = 1; sequence_number <= 100;
      ++sequence_number) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
    EXPECT_TRUE(manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA).IsZero());
    manager_->SentPacket(
        sequence_number, clock_.Now(), 1000, NOT_RETRANSMISSION);
    // Ack the packet we sent.
    ack.received_info.largest_observed = sequence_number;
    manager_->OnIncomingAckFrame(ack, clock_.Now());
  }
  EXPECT_EQ(100000, manager_->BandwidthEstimate().ToBytesPerSecond());
  EXPECT_NEAR(100000,
              manager_->SentBandwidth(clock_.Now()).ToBytesPerSecond(), 2000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(500));
  EXPECT_NEAR(50000,
              manager_->SentBandwidth(clock_.Now()).ToBytesPerSecond(), 1000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(501));
  EXPECT_NEAR(100000, manager_->BandwidthEstimate().ToBytesPerSecond(), 2000);
  EXPECT_TRUE(manager_->SentBandwidth(clock_.Now()).IsZero());
  for (int i = 1; i <= 150; ++i) {
    EXPECT_TRUE(manager_->TimeUntilSend(
        clock_.Now(), NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA).IsZero());
    manager_->SentPacket(i + 100, clock_.Now(), 1000, NOT_RETRANSMISSION);
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
    // Ack the packet we sent.
    ack.received_info.largest_observed = i + 100;
    manager_->OnIncomingAckFrame(ack, clock_.Now());
  }
  EXPECT_EQ(100, manager_->BandwidthEstimate().ToKBytesPerSecond());
  EXPECT_NEAR(100,
              manager_->SentBandwidth(clock_.Now()).ToKBytesPerSecond(), 2);
}

TEST_F(QuicCongestionManagerTest, Rtt) {
  SetUpCongestionType(kFixRate);

  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  manager_->SetSendAlgorithm(send_algorithm);

  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(15);

  EXPECT_CALL(*send_algorithm, SentPacket(_, _, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm,
              OnIncomingAck(sequence_number, _, expected_rtt)).Times(1);

  manager_->SentPacket(sequence_number, clock_.Now(), 1000, NOT_RETRANSMISSION);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  manager_->OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_->rtt(), expected_rtt);
}

TEST_F(QuicCongestionManagerTest, RttWithInvalidDelta) {
  // Expect that the RTT is infinite since the delta_time_largest_observed is
  // larger than the local time elapsed aka invalid.
  SetUpCongestionType(kFixRate);

  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  manager_->SetSendAlgorithm(send_algorithm);

  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::Infinite();

  EXPECT_CALL(*send_algorithm, SentPacket(_, _, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm,
              OnIncomingAck(sequence_number, _, expected_rtt)).Times(1);

  manager_->SentPacket(sequence_number, clock_.Now(), 1000, NOT_RETRANSMISSION);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(11);
  manager_->OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_->rtt(), expected_rtt);
}

TEST_F(QuicCongestionManagerTest, RttInfiniteDelta) {
  // Expect that the RTT is infinite since the delta_time_largest_observed is
  // infinite aka invalid.
  SetUpCongestionType(kFixRate);

  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  manager_->SetSendAlgorithm(send_algorithm);

  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::Infinite();

  EXPECT_CALL(*send_algorithm, SentPacket(_, _, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm,
              OnIncomingAck(sequence_number, _, expected_rtt)).Times(1);

  manager_->SentPacket(sequence_number, clock_.Now(), 1000, NOT_RETRANSMISSION);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed = QuicTime::Delta::Infinite();
  manager_->OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_->rtt(), expected_rtt);
}

TEST_F(QuicCongestionManagerTest, RttZeroDelta) {
  // Expect that the RTT is the time between send and receive since the
  // delta_time_largest_observed is zero.
  SetUpCongestionType(kFixRate);

  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  manager_->SetSendAlgorithm(send_algorithm);

  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);

  EXPECT_CALL(*send_algorithm, SentPacket(_, _, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm,
              OnIncomingAck(sequence_number, _, expected_rtt)).Times(1);

  manager_->SentPacket(sequence_number, clock_.Now(), 1000, NOT_RETRANSMISSION);
  clock_.AdvanceTime(expected_rtt);

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed = QuicTime::Delta::Zero();
  manager_->OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_->rtt(), expected_rtt);
}

}  // namespace test
}  // namespace net
