// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/quic_congestion_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/inter_arrival_sender.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;
using testing::Return;

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

  QuicTime::Delta rtt() {
    return rtt_sample_;
  }

  size_t GetNackCount(QuicPacketSequenceNumber sequence_number) const {
    return packet_history_map_.find(sequence_number)->second->nack_count();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCongestionManagerPeer);
};

class QuicCongestionManagerTest : public ::testing::Test {
 protected:
  QuicCongestionManagerTest()
      : manager_(&clock_, kFixRate),
        send_algorithm_(new StrictMock<MockSendAlgorithm>) {
    manager_.SetSendAlgorithm(send_algorithm_);
  }

  static const HasRetransmittableData kIgnored = HAS_RETRANSMITTABLE_DATA;

  MockClock clock_;
  QuicCongestionManagerPeer manager_;
  MockSendAlgorithm* send_algorithm_;
};

TEST_F(QuicCongestionManagerTest, NackRetransmit1Packet) {
  const size_t kNumSentPackets = 4;
  // Transmit 4 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 3 times with increasing largest observed.
  QuicAckFrame ack;
  ack.received_info.delta_time_largest_observed =
        QuicTime::Delta::FromMilliseconds(5);
  ack.received_info.missing_packets.insert(1);
  for (size_t i = 1; i <= 3; ++i) {
    ack.received_info.largest_observed = i + 1;
    EXPECT_CALL(*send_algorithm_, OnPacketAcked(i + 1, _, _)).Times(1);
    if (i == 3) {
      EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
      EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
    }
    SequenceNumberSet retransmissions =
        manager_.OnIncomingAckFrame(ack, clock_.Now());
    EXPECT_EQ(i == 3 ? 1u : 0u, retransmissions.size());
    EXPECT_EQ(i, manager_.GetNackCount(1));
  }
}

// A stretch ack is an ack that covers more than 1 packet of previously
// unacknowledged data.
TEST_F(QuicCongestionManagerTest, NackRetransmit1PacketWith1StretchAck) {
  const size_t kNumSentPackets = 4;
  // Transmit 4 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 3 times in a single StretchAck.
  QuicAckFrame ack;
  ack.received_info.delta_time_largest_observed =
        QuicTime::Delta::FromMilliseconds(5);
  ack.received_info.missing_packets.insert(1);
  ack.received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(_, _, _)).Times(3);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(3u, manager_.GetNackCount(1));
}

// Ack a packet 3 packets ahead, causing a retransmit.
TEST_F(QuicCongestionManagerTest, NackRetransmit1PacketSingleAck) {
  const size_t kNumSentPackets = 4;
  // Transmit 4 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 3 times in an AckFrame with three missing packets.
  QuicAckFrame ack;
  ack.received_info.delta_time_largest_observed =
        QuicTime::Delta::FromMilliseconds(5);
  ack.received_info.missing_packets.insert(1);
  ack.received_info.missing_packets.insert(2);
  ack.received_info.missing_packets.insert(3);
  ack.received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(3u, manager_.GetNackCount(1));
}

TEST_F(QuicCongestionManagerTest, EarlyRetransmit1Packet) {
  const size_t kNumSentPackets = 2;
  // Transmit 2 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Early retransmit when the final packet gets acked and the first is nacked.
  QuicAckFrame ack;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  ack.received_info.missing_packets.insert(1);
  ack.received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(1u, manager_.GetNackCount(1));
}

TEST_F(QuicCongestionManagerTest, DontEarlyRetransmitPacket) {
  const size_t kNumSentPackets = 4;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Fast retransmit when the final packet gets acked, but don't early
  // retransmit as well, because there are 4 packets outstanding when the ack
  // arrives.
  QuicAckFrame ack;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  ack.received_info.missing_packets.insert(1);
  ack.received_info.missing_packets.insert(2);
  ack.received_info.missing_packets.insert(3);
  ack.received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(3u, manager_.GetNackCount(1));
}

TEST_F(QuicCongestionManagerTest, NackRetransmit10Packets) {
  const size_t kNumSentPackets = 20;
  // Transmit 20 packets.
  for (QuicPacketSequenceNumber i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first 19 packets 3 times.
  QuicAckFrame ack;
  ack.received_info.largest_observed = kNumSentPackets;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    ack.received_info.missing_packets.insert(i);
  }
  EXPECT_CALL(*send_algorithm_,
              OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(_, _)).Times(10);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(_, _)).Times(10);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(10u, retransmissions.size());
  for (size_t j = 1; j < kNumSentPackets; ++j) {
    EXPECT_EQ(kNumSentPackets - j, manager_.GetNackCount(j));
  }
}

TEST_F(QuicCongestionManagerTest, NackRetransmit10PacketsAlternateAcks) {
  const size_t kNumSentPackets = 30;
  // Transmit 15 packets of data and 15 ack packets.  The send algorithm will
  // inform the congestion manager not to save the acks by returning false.
  for (QuicPacketSequenceNumber i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(i % 2 == 0 ? false : true));
    manager_.OnPacketSent(
        i, clock_.Now(), 1000, NOT_RETRANSMISSION,
        i % 2 == 0 ? NO_RETRANSMITTABLE_DATA : HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first 29 packets 3 times.
  QuicAckFrame ack;
  ack.received_info.largest_observed = kNumSentPackets;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    ack.received_info.missing_packets.insert(i);
  }
  // We never actually get an ack call, since the kNumSentPackets packet was
  // not saved.
  EXPECT_CALL(*send_algorithm_, OnPacketLost(_, _)).Times(10);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(_, _)).Times(10);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(10u, retransmissions.size());
  // Only non-ack packets have a nack count.
  for (size_t j = 1; j < kNumSentPackets; j += 2) {
    EXPECT_EQ(kNumSentPackets - j, manager_.GetNackCount(j));
  }

  // Ensure only the odd packets were retransmitted, since the others were not
  // retransmittable(ie: acks).
  for (SequenceNumberSet::const_iterator it = retransmissions.begin();
       it != retransmissions.end(); ++it) {
    EXPECT_EQ(1u, *it % 2);
  }
}

TEST_F(QuicCongestionManagerTest, NackTwiceThenAck) {
  // Transmit 4 packets.
  for (QuicPacketSequenceNumber i = 1; i <= 4; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 2 times, then ack it.
  QuicAckFrame ack;
  ack.received_info.missing_packets.insert(1);
  for (size_t i = 1; i <= 3; ++i) {
    if (i == 3) {
      ack.received_info.missing_packets.clear();
    }
    ack.received_info.largest_observed = i + 1;
    ack.received_info.delta_time_largest_observed =
        QuicTime::Delta::FromMilliseconds(5);
    EXPECT_CALL(*send_algorithm_,
                OnPacketAcked(_, _, _)).Times(i == 3 ? 2 : 1);
    SequenceNumberSet retransmissions =
        manager_.OnIncomingAckFrame(ack, clock_.Now());
    EXPECT_EQ(0u, retransmissions.size());
    // The nack count remains at 2 when the packet is acked.
    EXPECT_EQ(i == 3 ? 2u : i, manager_.GetNackCount(1));
  }
}

TEST_F(QuicCongestionManagerTest, Rtt) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(15);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                               .Times(1).WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm_,
              OnPacketAcked(sequence_number, _, expected_rtt)).Times(1);

  manager_.OnPacketSent(sequence_number, clock_.Now(), 1000,
                         NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_.rtt(), expected_rtt);
}

TEST_F(QuicCongestionManagerTest, RttWithInvalidDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // delta_time_largest_observed is larger than the local time elapsed
  // and is hence invalid.
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                               .Times(1).WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm_,
              OnPacketAcked(sequence_number, _, expected_rtt)).Times(1);

  manager_.OnPacketSent(sequence_number, clock_.Now(), 1000,
                         NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(11);
  manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_.rtt(), expected_rtt);
}

TEST_F(QuicCongestionManagerTest, RttWithInfiniteDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // delta_time_largest_observed is infinite, and is hence invalid.
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                               .Times(1).WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm_,
              OnPacketAcked(sequence_number, _, expected_rtt)).Times(1);

  manager_.OnPacketSent(sequence_number, clock_.Now(), 1000,
                         NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed = QuicTime::Delta::Infinite();
  manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_.rtt(), expected_rtt);
}

TEST_F(QuicCongestionManagerTest, RttZeroDelta) {
  // Expect that the RTT is the time between send and receive since the
  // delta_time_largest_observed is zero.
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                               .Times(1).WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(sequence_number, _, expected_rtt))
      .Times(1);

  manager_.OnPacketSent(sequence_number, clock_.Now(), 1000,
                         NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  clock_.AdvanceTime(expected_rtt);

  QuicAckFrame ack;
  ack.received_info.largest_observed = sequence_number;
  ack.received_info.delta_time_largest_observed = QuicTime::Delta::Zero();
  manager_.OnIncomingAckFrame(ack, clock_.Now());
  EXPECT_EQ(manager_.rtt(), expected_rtt);
}

TEST_F(QuicCongestionManagerTest, RetransmissionTimeout) {
  // Send 100 packets and then ensure all are abandoned when the RTO fires.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                          NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(_, _)).Times(kNumSentPackets);

  manager_.OnRetransmissionTimeout();
}

TEST_F(QuicCongestionManagerTest, GetTransmissionDelayMin) {
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(1);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(delay));

  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            manager_.GetRetransmissionDelay());
}

TEST_F(QuicCongestionManagerTest, GetTransmissionDelayMax) {
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
  QuicTime::Delta delay = QuicTime::Delta::FromSeconds(500);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(delay));

  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::FromSeconds(60),
            manager_.GetRetransmissionDelay());
}

TEST_F(QuicCongestionManagerTest, GetTransmissionDelay) {
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(500);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillRepeatedly(Return(delay));

  // Delay should back off exponentially.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay, manager_.GetRetransmissionDelay());
    delay = delay.Add(delay);
    EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
    manager_.OnRetransmissionTimeout();
  }
}

TEST_F(QuicCongestionManagerTest, GetTestTransmissionDelayTailDrop) {
  FLAGS_limit_rto_increase_for_tests = true;

  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(500);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillRepeatedly(Return(delay));

  // No backoff for the first 5 retransmissions.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay, manager_.GetRetransmissionDelay());
    EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
    manager_.OnRetransmissionTimeout();
  }

  // Then backoff starts.
  EXPECT_EQ(delay.Add(delay), manager_.GetRetransmissionDelay());
}

}  // namespace test
}  // namespace net
