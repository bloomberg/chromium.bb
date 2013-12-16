// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/stl_util.h"
#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::vector;
using testing::_;
using testing::Return;
using testing::StrictMock;

namespace net {
namespace test {
namespace {

class MockHelper : public QuicSentPacketManager::HelperInterface {
 public:
  MOCK_METHOD0(GetNextPacketSequenceNumber, QuicPacketSequenceNumber());
};

class QuicSentPacketManagerTest : public ::testing::TestWithParam<bool> {
 protected:
  QuicSentPacketManagerTest()
      : manager_(true, &helper_, &clock_, kFixRate),
        send_algorithm_(new StrictMock<MockSendAlgorithm>) {
    QuicSentPacketManagerPeer::SetSendAlgorithm(&manager_, send_algorithm_);
  }

  ~QuicSentPacketManagerTest() {
    STLDeleteElements(&packets_);
  }

  void VerifyUnackedPackets(QuicPacketSequenceNumber* packets,
                            size_t num_packets) {
    if (num_packets == 0) {
      EXPECT_FALSE(manager_.HasUnackedPackets());
      EXPECT_EQ(0u, manager_.GetNumRetransmittablePackets());
      return;
    }

    EXPECT_TRUE(manager_.HasUnackedPackets());
    EXPECT_EQ(packets[0], manager_.GetLeastUnackedSentPacket());
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(manager_.IsUnacked(packets[i])) << packets[i];
    }
  }

  void VerifyRetransmittablePackets(QuicPacketSequenceNumber* packets,
                                    size_t num_packets) {
    SequenceNumberSet unacked = manager_.GetUnackedPackets();
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(ContainsKey(unacked, packets[i])) << packets[i];
    }
    size_t num_retransmittable = 0;
    for (SequenceNumberSet::const_iterator it = unacked.begin();
         it != unacked.end(); ++it) {
      if (manager_.HasRetransmittableFrames(*it)) {
        ++num_retransmittable;
      }
    }
    EXPECT_EQ(num_packets, manager_.GetNumRetransmittablePackets());
    EXPECT_EQ(num_packets, num_retransmittable);
  }

  void VerifyAckedPackets(QuicPacketSequenceNumber* expected,
                          size_t num_expected,
                          const SequenceNumberSet& actual) {
    if (num_expected == 0) {
      EXPECT_TRUE(actual.empty());
      return;
    }

    EXPECT_EQ(num_expected, actual.size());
    for (size_t i = 0; i < num_expected; ++i) {
      EXPECT_TRUE(ContainsKey(actual, expected[i])) << expected[i];
    }
  }

  void RetransmitPacket(QuicPacketSequenceNumber old_sequence_number,
                        QuicPacketSequenceNumber new_sequence_number) {
    QuicSentPacketManagerPeer::MarkForRetransmission(
        &manager_, old_sequence_number, NACK_RETRANSMISSION);
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    QuicSentPacketManager::PendingRetransmission next_retransmission =
        manager_.NextPendingRetransmission();
    EXPECT_EQ(old_sequence_number, next_retransmission.sequence_number);
    EXPECT_EQ(NACK_RETRANSMISSION, next_retransmission.transmission_type);
    manager_.OnRetransmittedPacket(old_sequence_number, new_sequence_number);
    EXPECT_TRUE(QuicSentPacketManagerPeer::IsRetransmission(
        &manager_, new_sequence_number));
  }

  SerializedPacket CreatePacket(QuicPacketSequenceNumber sequence_number) {
    packets_.push_back(QuicPacket::NewDataPacket(
        NULL, 0, false, PACKET_8BYTE_GUID, false,
        PACKET_6BYTE_SEQUENCE_NUMBER));
    return SerializedPacket(sequence_number, PACKET_6BYTE_SEQUENCE_NUMBER,
                            packets_.back(), 0u, new RetransmittableFrames());
  }

  SerializedPacket CreateFecPacket(QuicPacketSequenceNumber sequence_number) {
    packets_.push_back(QuicPacket::NewFecPacket(
        NULL, 0, false, PACKET_8BYTE_GUID, false,
        PACKET_6BYTE_SEQUENCE_NUMBER));
    return SerializedPacket(sequence_number, PACKET_6BYTE_SEQUENCE_NUMBER,
                            packets_.back(), 0u, NULL);
  }

  void SendDataPacket(QuicPacketSequenceNumber sequence_number) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, sequence_number, _, _, _))
                    .Times(1).WillOnce(Return(true));
    SerializedPacket packet(CreatePacket(sequence_number));
    manager_.OnSerializedPacket(packet);
    manager_.OnPacketSent(sequence_number, clock_.ApproximateNow(),
                          packet.packet->length(), NOT_RETRANSMISSION,
                          HAS_RETRANSMITTABLE_DATA);
  }

  // Based on QuicConnection's WritePendingRetransmissions.
  void RetransmitNextPacket(
      QuicPacketSequenceNumber retransmission_sequence_number) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, retransmission_sequence_number, _, _, _))
                    .Times(1).WillOnce(Return(true));
    const QuicSentPacketManager::PendingRetransmission pending =
        manager_.NextPendingRetransmission();
    manager_.OnRetransmittedPacket(
        pending.sequence_number, retransmission_sequence_number);
    manager_.OnPacketSent(retransmission_sequence_number,
                          clock_.ApproximateNow(), 1000,
                          pending.transmission_type, HAS_RETRANSMITTABLE_DATA);
  }

  testing::StrictMock<MockHelper> helper_;
  QuicSentPacketManager manager_;
  vector<QuicPacket*> packets_;
  MockClock clock_;
  MockSendAlgorithm* send_algorithm_;
};

TEST_F(QuicSentPacketManagerTest, IsUnacked) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);

  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 1 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, IsUnAckedRetransmit) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  RetransmitPacket(1, 2);

  EXPECT_TRUE(QuicSentPacketManagerPeer::IsRetransmission(&manager_, 2));
  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 2 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAck) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  RetransmitPacket(1, 2);

  // Ack 2 but not 1.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  manager_.OnIncomingAck(received_info, QuicTime::Zero());

  // No unacked packets remain.
  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckBeforeSend) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  QuicSentPacketManagerPeer::MarkForRetransmission(
      &manager_, 1, NACK_RETRANSMISSION);
  EXPECT_TRUE(manager_.HasPendingRetransmissions());

  // Ack 1.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, QuicTime::Zero());

  // There should no longer be a pending retransmission.
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // No unacked packets remain.
  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicSentPacketManagerTest, RetransmitThenAckPrevious) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  RetransmitPacket(1, 2);

  // Ack 1 but not 2.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, QuicTime::Zero());

  // 2 remains unacked, but no packets have retransmittable data.
  QuicPacketSequenceNumber unacked[] = { 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  // Verify that if the retransmission alarm does fire to abandon packet 2,
  // the sent packet manager is not notified, since there is no retransmittable
  // data outstanding.
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(QuicTime::Delta::FromMilliseconds(1)));
  manager_.OnRetransmissionTimeout();
}

TEST_F(QuicSentPacketManagerTest, RetransmitTwiceThenAckFirst) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  RetransmitPacket(1, 2);
  RetransmitPacket(2, 3);

  // Ack 1 but not 2 or 3.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, QuicTime::Zero());

  // 3 remains unacked, but no packets have retransmittable data.
  QuicPacketSequenceNumber unacked[] = { 3 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  // Verify that if the retransmission alarm does fire to abandon packet 3,
  // the sent packet manager is not notified, since there is no retransmittable
  // data outstanding.
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(QuicTime::Delta::FromMilliseconds(1)));
  manager_.OnRetransmissionTimeout();
}

TEST_F(QuicSentPacketManagerTest, TruncatedAck) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  RetransmitPacket(1, 2);
  RetransmitPacket(2, 3);
  RetransmitPacket(3, 4);

  // Truncated ack with 2 NACKs
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  received_info.is_truncated = true;
  manager_.OnIncomingAck(received_info, QuicTime::Zero());

  // High water mark will be raised.
  QuicPacketSequenceNumber unacked[] = { 2, 3, 4 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 4 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_F(QuicSentPacketManagerTest, SendDropAckRetransmitManyPackets) {
  manager_.OnSerializedPacket(CreatePacket(1));
  manager_.OnSerializedPacket(CreatePacket(2));
  manager_.OnSerializedPacket(CreatePacket(3));

  {
    // Ack packets 1 and 3.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 3;
    received_info.missing_packets.insert(2);
    manager_.OnIncomingAck(received_info, QuicTime::Zero());

    QuicPacketSequenceNumber unacked[] = { 2 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 2 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  manager_.OnSerializedPacket(CreatePacket(4));
  manager_.OnSerializedPacket(CreatePacket(5));

  {
    // Ack packets 5.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 5;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    manager_.OnIncomingAck(received_info, QuicTime::Zero());

    QuicPacketSequenceNumber unacked[] = { 2, 4 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 2, 4 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  manager_.OnSerializedPacket(CreatePacket(6));
  manager_.OnSerializedPacket(CreatePacket(7));

  {
    // Ack packets 7.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 7;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    received_info.missing_packets.insert(6);
    manager_.OnIncomingAck(received_info, QuicTime::Zero());

    QuicPacketSequenceNumber unacked[] = { 2, 4, 6 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 2, 4, 6 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  RetransmitPacket(2, 8);
  manager_.OnSerializedPacket(CreatePacket(9));
  manager_.OnSerializedPacket(CreatePacket(10));

  {
    // Ack packet 10.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 10;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    received_info.missing_packets.insert(6);
    received_info.missing_packets.insert(8);
    received_info.missing_packets.insert(9);
    manager_.OnIncomingAck(received_info, QuicTime::Zero());

    QuicPacketSequenceNumber unacked[] = { 2, 4, 6, 8, 9 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 4, 6, 8, 9 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }


  RetransmitPacket(4, 11);
  manager_.OnSerializedPacket(CreatePacket(12));
  manager_.OnSerializedPacket(CreatePacket(13));

  {
    // Ack packet 13.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 13;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    received_info.missing_packets.insert(6);
    received_info.missing_packets.insert(8);
    received_info.missing_packets.insert(9);
    received_info.missing_packets.insert(11);
    received_info.missing_packets.insert(12);
    manager_.OnIncomingAck(received_info, QuicTime::Zero());

    QuicPacketSequenceNumber unacked[] = { 2, 4, 6, 8, 9, 11, 12 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 6, 8, 9, 11, 12 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  RetransmitPacket(6, 14);
  manager_.OnSerializedPacket(CreatePacket(15));
  manager_.OnSerializedPacket(CreatePacket(16));

  {
    // Ack packet 16.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 13;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    received_info.missing_packets.insert(6);
    received_info.missing_packets.insert(8);
    received_info.missing_packets.insert(9);
    received_info.missing_packets.insert(11);
    received_info.missing_packets.insert(12);
    received_info.is_truncated = true;
    manager_.OnIncomingAck(received_info, QuicTime::Zero());

    // Truncated ack raises the high water mark by clearing out 2, 4, and 6.
    QuicPacketSequenceNumber unacked[] = { 8, 9, 11, 12, 14, 15, 16 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 8, 9, 11, 12, 14, 15, 16 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacket) {
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(1u));
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnacked) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnackedFec) {
  SerializedPacket serialized_packet(CreateFecPacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketDiscardUnacked) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet);
  manager_.DiscardUnackedPacket(1u);
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(2u));
  EXPECT_EQ(2u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedPacketAndDiscard) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreateFecPacket(1));
  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());

  SerializedPacket serialized_packet2(CreateFecPacket(2));
  manager_.OnSerializedPacket(serialized_packet2);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());

  SerializedPacket serialized_packet3(CreateFecPacket(3));
  manager_.OnSerializedPacket(serialized_packet3);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());

  QuicPacketSequenceNumber unacked[] = { 1, 2, 3 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  manager_.DiscardUnackedPacket(1);
  EXPECT_EQ(2u, manager_.GetLeastUnackedSentPacket());

  // Ack 2.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  manager_.OnIncomingAck(received_info, QuicTime::Zero());

  EXPECT_EQ(3u, manager_.GetLeastUnackedSentPacket());

  // Discard the 3rd packet and ensure there are no FEC packets.
  manager_.DiscardUnackedPacket(3);
  EXPECT_FALSE(manager_.HasUnackedPackets());
}

TEST_F(QuicSentPacketManagerTest, GetSentTime) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreateFecPacket(1));
  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, 1, _, _, _))
                  .Times(1).WillOnce(Return(true));
  manager_.OnPacketSent(
      1, QuicTime::Zero(), 0, NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  SerializedPacket serialized_packet2(CreateFecPacket(2));
  QuicTime sent_time = QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(1));
  manager_.OnSerializedPacket(serialized_packet2);
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, 2, _, _, _))
                  .Times(1).WillOnce(Return(true));
  manager_.OnPacketSent(
      2, sent_time, 0, NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);

  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  EXPECT_TRUE(manager_.HasUnackedPackets());
  EXPECT_EQ(QuicTime::Zero(),
            QuicSentPacketManagerPeer::GetSentTime(&manager_, 1));
  EXPECT_EQ(sent_time, QuicSentPacketManagerPeer::GetSentTime(&manager_, 2));
}

TEST_F(QuicSentPacketManagerTest, NackRetransmit1Packet) {
  const size_t kNumSentPackets = 4;
  // Transmit 4 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 3 times with increasing largest observed.
  ReceivedPacketInfo received_info;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  received_info.missing_packets.insert(1);
  for (QuicPacketSequenceNumber i = 1; i <= 3; ++i) {
    received_info.largest_observed = i + 1;
    EXPECT_CALL(*send_algorithm_, OnPacketAcked(i + 1, _, _)).Times(1);
    if (i == 3) {
      EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
      EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
    }
    SequenceNumberSet retransmissions =
        manager_.OnIncomingAckFrame(received_info, clock_.Now());
    EXPECT_EQ(i == 3 ? 1u : 0u, retransmissions.size());
    EXPECT_EQ(i, QuicSentPacketManagerPeer::GetNackCount(&manager_, 1));
  }
}

// A stretch ack is an ack that covers more than 1 packet of previously
// unacknowledged data.
TEST_F(QuicSentPacketManagerTest, NackRetransmit1PacketWith1StretchAck) {
  const size_t kNumSentPackets = 4;
  // Transmit 4 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 3 times in a single StretchAck.
  ReceivedPacketInfo received_info;
  received_info.delta_time_largest_observed =
        QuicTime::Delta::FromMilliseconds(5);
  received_info.missing_packets.insert(1);
  received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(_, _, _)).Times(3);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(3u, QuicSentPacketManagerPeer::GetNackCount(&manager_, 1));
}

// Ack a packet 3 packets ahead, causing a retransmit.
TEST_F(QuicSentPacketManagerTest, NackRetransmit1PacketSingleAck) {
  const size_t kNumSentPackets = 4;
  // Transmit 4 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 3 times in an AckFrame with three missing packets.
  ReceivedPacketInfo received_info;
  received_info.delta_time_largest_observed =
        QuicTime::Delta::FromMilliseconds(5);
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  received_info.missing_packets.insert(3);
  received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(3u, QuicSentPacketManagerPeer::GetNackCount(&manager_, 1));
}

TEST_F(QuicSentPacketManagerTest, EarlyRetransmit1Packet) {
  const size_t kNumSentPackets = 2;
  // Transmit 2 packets.
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Early retransmit when the final packet gets acked and the first is nacked.
  ReceivedPacketInfo received_info;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  received_info.missing_packets.insert(1);
  received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(1u, QuicSentPacketManagerPeer::GetNackCount(&manager_, 1));
}

TEST_F(QuicSentPacketManagerTest, DontEarlyRetransmitPacket) {
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
  ReceivedPacketInfo received_info;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  received_info.missing_packets.insert(3);
  received_info.largest_observed = kNumSentPackets;
  EXPECT_CALL(*send_algorithm_, OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(1, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, _)).Times(1);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(1u, retransmissions.size());
  EXPECT_EQ(3u, QuicSentPacketManagerPeer::GetNackCount(&manager_, 1));
}

TEST_F(QuicSentPacketManagerTest, NackRetransmit2Packets) {
  const size_t kNumSentPackets = 20;
  // Transmit 20 packets.
  for (QuicPacketSequenceNumber i = 1; i <= kNumSentPackets; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first 19 packets 3 times.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = kNumSentPackets;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    received_info.missing_packets.insert(i);
  }
  EXPECT_CALL(*send_algorithm_,
              OnPacketAcked(kNumSentPackets, _, _)).Times(1);
  EXPECT_CALL(*send_algorithm_, OnPacketLost(_, _)).Times(2);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(_, _)).Times(2);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(2u, retransmissions.size());
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    EXPECT_EQ(kNumSentPackets - i,
              QuicSentPacketManagerPeer::GetNackCount(&manager_, i));
  }
}

TEST_F(QuicSentPacketManagerTest, NackRetransmit2PacketsAlternateAcks) {
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
  ReceivedPacketInfo received_info;
  received_info.largest_observed = kNumSentPackets;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  for (size_t i = 1; i < kNumSentPackets; ++i) {
    received_info.missing_packets.insert(i);
  }
  // We never actually get an ack call, since the kNumSentPackets packet was
  // not saved.
  EXPECT_CALL(*send_algorithm_, OnPacketLost(_, _)).Times(2);
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(_, _)).Times(2);
  SequenceNumberSet retransmissions =
      manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(2u, retransmissions.size());
  // Only non-ack packets have a nack count.
  for (size_t i = 1; i < kNumSentPackets; i += 2) {
    EXPECT_EQ(kNumSentPackets - i,
              QuicSentPacketManagerPeer::GetNackCount(&manager_, i));
  }

  // Ensure only the odd packets were retransmitted, since the others were not
  // retransmittable(ie: acks).
  for (SequenceNumberSet::const_iterator it = retransmissions.begin();
       it != retransmissions.end(); ++it) {
    EXPECT_EQ(1u, *it % 2);
  }
}

TEST_F(QuicSentPacketManagerTest, NackTwiceThenAck) {
  // Transmit 4 packets.
  for (QuicPacketSequenceNumber i = 1; i <= 4; ++i) {
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                    .Times(1).WillOnce(Return(true));
    manager_.OnPacketSent(i, clock_.Now(), 1000,
                           NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  // Nack the first packet 2 times, then ack it.
  ReceivedPacketInfo received_info;
  received_info.missing_packets.insert(1);
  for (size_t i = 1; i <= 3; ++i) {
    if (i == 3) {
      received_info.missing_packets.clear();
    }
    received_info.largest_observed = i + 1;
    received_info.delta_time_largest_observed =
        QuicTime::Delta::FromMilliseconds(5);
    EXPECT_CALL(*send_algorithm_,
                OnPacketAcked(_, _, _)).Times(i == 3 ? 2 : 1);
    SequenceNumberSet retransmissions =
        manager_.OnIncomingAckFrame(received_info, clock_.Now());
    EXPECT_EQ(0u, retransmissions.size());
    // The nack count remains at 2 when the packet is acked.
    EXPECT_EQ(i == 3 ? 2u : i,
              QuicSentPacketManagerPeer::GetNackCount(&manager_, 1));
  }
}

TEST_F(QuicSentPacketManagerTest, Rtt) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(15);

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
                               .Times(1).WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm_,
              OnPacketAcked(sequence_number, _, expected_rtt)).Times(1);

  manager_.OnPacketSent(sequence_number, clock_.Now(), 1000,
                         NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(5);
  manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt, QuicSentPacketManagerPeer::rtt(&manager_));
}

TEST_F(QuicSentPacketManagerTest, RttWithInvalidDelta) {
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

  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed =
      QuicTime::Delta::FromMilliseconds(11);
  manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt, QuicSentPacketManagerPeer::rtt(&manager_));
}

TEST_F(QuicSentPacketManagerTest, RttWithInfiniteDelta) {
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

  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed = QuicTime::Delta::Infinite();
  manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt, QuicSentPacketManagerPeer::rtt(&manager_));
}

TEST_F(QuicSentPacketManagerTest, RttZeroDelta) {
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

  ReceivedPacketInfo received_info;
  received_info.largest_observed = sequence_number;
  received_info.delta_time_largest_observed = QuicTime::Delta::Zero();
  manager_.OnIncomingAckFrame(received_info, clock_.Now());
  EXPECT_EQ(expected_rtt, QuicSentPacketManagerPeer::rtt(&manager_));
}

TEST_F(QuicSentPacketManagerTest, RetransmissionTimeout) {
  // Send 100 packets and then ensure all are abandoned when the RTO fires.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(_, _)).Times(kNumSentPackets);

  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(QuicTime::Delta::FromMilliseconds(1)));
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
  manager_.OnRetransmissionTimeout();
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionDelayMin) {
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(QuicTime::Delta::FromMilliseconds(1)));

  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            manager_.GetRetransmissionDelay());
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionDelayMax) {
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillOnce(Return(QuicTime::Delta::FromSeconds(500)));

  EXPECT_EQ(QuicTime::Delta::FromSeconds(60),
            manager_.GetRetransmissionDelay());
}

TEST_F(QuicSentPacketManagerTest, GetTransmissionDelay) {
  SendDataPacket(1);
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(500);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillRepeatedly(Return(delay));

  // Delay should back off exponentially.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay, manager_.GetRetransmissionDelay());
    delay = delay.Add(delay);
    EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(i + 1, _));
    EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
    manager_.OnRetransmissionTimeout();
    RetransmitNextPacket(i + 2);
  }
}

TEST_F(QuicSentPacketManagerTest, GetTestTransmissionDelayTailDrop) {
  FLAGS_limit_rto_increase_for_tests = true;

  SendDataPacket(1);
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(500);
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
      .WillRepeatedly(Return(delay));

  // No backoff for the first 5 retransmissions.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay, manager_.GetRetransmissionDelay());
    EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(i + 1, _));
    EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout());
    manager_.OnRetransmissionTimeout();
    RetransmitNextPacket(i + 2);
  }

  // Then backoff starts
  EXPECT_EQ(delay.Add(delay), manager_.GetRetransmissionDelay());
}

}  // namespace
}  // namespace test
}  // namespace net
