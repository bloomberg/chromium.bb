// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "base/stl_util.h"
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
  MOCK_METHOD2(OnPacketNacked, void(QuicPacketSequenceNumber sequence_number,
                                    size_t nack_count));
};

class QuicSentPacketManagerTest : public ::testing::TestWithParam<bool> {
 protected:
  QuicSentPacketManagerTest()
      : manager_(true, &helper_) {
  }

  void SetUp() {
    FLAGS_track_retransmission_history = GetParam();
  }

  ~QuicSentPacketManagerTest() {
    STLDeleteElements(&packets_);
  }


  void VerifyUnackedPackets(QuicPacketSequenceNumber* packets,
                            size_t num_packets) {
    if (num_packets == 0) {
      EXPECT_FALSE(manager_.HasUnackedPackets());
      EXPECT_EQ(0u, manager_.GetNumUnackedPackets());
      EXPECT_TRUE(manager_.GetUnackedPackets().empty());
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
    manager_.MarkForRetransmission(old_sequence_number, NACK_RETRANSMISSION);
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    QuicSentPacketManager::PendingRetransmission next_retransmission =
        manager_.NextPendingRetransmission();
    EXPECT_EQ(old_sequence_number, next_retransmission.sequence_number);
    EXPECT_EQ(NACK_RETRANSMISSION, next_retransmission.transmission_type);
    manager_.OnRetransmittedPacket(old_sequence_number, new_sequence_number);
    EXPECT_TRUE(manager_.IsRetransmission(new_sequence_number));
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

  testing::StrictMock<MockHelper> helper_;
  QuicSentPacketManager manager_;
  vector<QuicPacket*> packets_;
};

INSTANTIATE_TEST_CASE_P(TrackRetransmissionHistory,
                        QuicSentPacketManagerTest,
                        ::testing::Values(false, true));

TEST_P(QuicSentPacketManagerTest, IsUnacked) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());

  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 1 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_P(QuicSentPacketManagerTest, IsUnAckedRetransmit) {
  if (!FLAGS_track_retransmission_history) {
    // This tests restransmission tracking specifically.
    return;
  }
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  RetransmitPacket(1, 2);

  EXPECT_EQ(1u, manager_.GetRetransmissionCount(2));
  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 2 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));

  RetransmitPacket(2, 3);
  EXPECT_EQ(2u, manager_.GetRetransmissionCount(3));
  QuicPacketSequenceNumber unacked2[] = { 1, 2, 3 };
  VerifyUnackedPackets(unacked2, arraysize(unacked2));
  QuicPacketSequenceNumber retransmittable2[] = { 3 };
  VerifyRetransmittablePackets(retransmittable2, arraysize(retransmittable2));
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenAck) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  RetransmitPacket(1, 2);

  // Ack 2 but not 1.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  manager_.OnIncomingAck(received_info, false);

  // No unacked packets remain.
  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenAckBeforeSend) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  manager_.MarkForRetransmission(1, NACK_RETRANSMISSION);
  EXPECT_TRUE(manager_.HasPendingRetransmissions());

  // Ack 1.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 1;
  manager_.OnIncomingAck(received_info, false);

  // There should no longer be a pending retransmission.
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // No unacked packets remain.
  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenAckPrevious) {
  if (!FLAGS_track_retransmission_history) {
    // This tests restransmission tracking specifically.
    return;
  }
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  RetransmitPacket(1, 2);

  // Ack 1 but not 2.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(2);
  EXPECT_CALL(helper_, OnPacketNacked(2, 1)).Times(1);
  manager_.OnIncomingAck(received_info, false);

  // 2 remains unacked, but no packets have retransmittable data.
  QuicPacketSequenceNumber unacked[] = { 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_P(QuicSentPacketManagerTest, TruncatedAck) {
  if (!FLAGS_track_retransmission_history) {
    // This tests restransmission tracking specifically.
    return;
  }
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  RetransmitPacket(1, 2);
  RetransmitPacket(2, 3);
  RetransmitPacket(3, 4);

  // Truncated ack with 2 NACKs
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  received_info.missing_packets.insert(1);
  received_info.missing_packets.insert(2);
  manager_.OnIncomingAck(received_info, true);

  // High water mark will be raised.
  QuicPacketSequenceNumber unacked[] = { 2, 3, 4 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 4 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
}

TEST_P(QuicSentPacketManagerTest, SendDropAckRetransmitManyPackets) {
  if (!FLAGS_track_retransmission_history) {
    // This tests restransmission tracking specifically.
    return;
  }
  manager_.OnSerializedPacket(CreatePacket(1), QuicTime::Zero());
  manager_.OnSerializedPacket(CreatePacket(2), QuicTime::Zero());
  manager_.OnSerializedPacket(CreatePacket(3), QuicTime::Zero());

  {
    // Ack packets 1 and 3.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 3;
    received_info.missing_packets.insert(2);
    EXPECT_CALL(helper_, OnPacketNacked(2, 1)).Times(1);
    manager_.OnIncomingAck(received_info, false);

    QuicPacketSequenceNumber unacked[] = { 2 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 2 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  manager_.OnSerializedPacket(CreatePacket(4), QuicTime::Zero());
  manager_.OnSerializedPacket(CreatePacket(5), QuicTime::Zero());

  {
    // Ack packets 5.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 5;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    EXPECT_CALL(helper_, OnPacketNacked(2, 2)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(4, 1)).Times(1);
    manager_.OnIncomingAck(received_info, false);

    QuicPacketSequenceNumber unacked[] = { 2, 4 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 2, 4 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  manager_.OnSerializedPacket(CreatePacket(6), QuicTime::Zero());
  manager_.OnSerializedPacket(CreatePacket(7), QuicTime::Zero());

  {
    // Ack packets 7.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 7;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    received_info.missing_packets.insert(6);
    EXPECT_CALL(helper_, OnPacketNacked(2, 3)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(4, 2)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(6, 1)).Times(1);
    manager_.OnIncomingAck(received_info, false);

    QuicPacketSequenceNumber unacked[] = { 2, 4, 6 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 2, 4, 6 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  RetransmitPacket(2, 8);
  manager_.OnSerializedPacket(CreatePacket(9), QuicTime::Zero());
  manager_.OnSerializedPacket(CreatePacket(10), QuicTime::Zero());

  {
    // Ack packet 10.
    ReceivedPacketInfo received_info;
    received_info.largest_observed = 10;
    received_info.missing_packets.insert(2);
    received_info.missing_packets.insert(4);
    received_info.missing_packets.insert(6);
    received_info.missing_packets.insert(8);
    received_info.missing_packets.insert(9);
    EXPECT_CALL(helper_, OnPacketNacked(4, 3)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(6, 2)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(8, 1)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(9, 1)).Times(1);
    manager_.OnIncomingAck(received_info, false);

    QuicPacketSequenceNumber unacked[] = { 2, 4, 6, 8, 9 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 4, 6, 8, 9 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }


  RetransmitPacket(4, 11);
  manager_.OnSerializedPacket(CreatePacket(12), QuicTime::Zero());
  manager_.OnSerializedPacket(CreatePacket(13), QuicTime::Zero());

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
    EXPECT_CALL(helper_, OnPacketNacked(6, 3)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(8, 2)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(9, 2)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(11, 1)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(12, 1)).Times(1);
    manager_.OnIncomingAck(received_info, false);

    QuicPacketSequenceNumber unacked[] = { 2, 4, 6, 8, 9, 11, 12 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 6, 8, 9, 11, 12 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }

  RetransmitPacket(6, 14);
  manager_.OnSerializedPacket(CreatePacket(15), QuicTime::Zero());
  manager_.OnSerializedPacket(CreatePacket(16), QuicTime::Zero());

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
    EXPECT_CALL(helper_, OnPacketNacked(8, 3)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(9, 3)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(11, 2)).Times(1);
    EXPECT_CALL(helper_, OnPacketNacked(12, 2)).Times(1);
    manager_.OnIncomingAck(received_info, true);

    // Truncated ack raises the high water mark by clearing out 2, 4, and 6.
    QuicPacketSequenceNumber unacked[] = { 8, 9, 11, 12, 14, 15, 16 };
    VerifyUnackedPackets(unacked, arraysize(unacked));
    QuicPacketSequenceNumber retransmittable[] = { 8, 9, 11, 12, 14, 15, 16 };
    VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  }
}

TEST_P(QuicSentPacketManagerTest, GetLeastUnackedSentPacket) {
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(1u));
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_P(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnacked) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_P(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnackedFec) {
  SerializedPacket serialized_packet(CreateFecPacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  // FEC packets do not count as "unacked".
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(2u));
  EXPECT_EQ(2u, manager_.GetLeastUnackedSentPacket());
}

TEST_P(QuicSentPacketManagerTest, GetLeastUnackedSentPacketDiscardUnacked) {
  SerializedPacket serialized_packet(CreatePacket(1));

  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  manager_.DiscardUnackedPacket(1u);
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(2u));
  EXPECT_EQ(2u, manager_.GetLeastUnackedSentPacket());
}

TEST_P(QuicSentPacketManagerTest, GetLeastUnackedFecPacketAndDiscard) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreateFecPacket(1));
  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  EXPECT_EQ(1u, manager_.GetLeastUnackedFecPacket());

  SerializedPacket serialized_packet2(CreateFecPacket(2));
  manager_.OnSerializedPacket(serialized_packet2, QuicTime::Zero());
  EXPECT_EQ(1u, manager_.GetLeastUnackedFecPacket());

  SerializedPacket serialized_packet3(CreateFecPacket(3));
  manager_.OnSerializedPacket(serialized_packet3, QuicTime::Zero());
  EXPECT_EQ(1u, manager_.GetLeastUnackedFecPacket());

  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);

  manager_.DiscardFecPacket(1);
  EXPECT_EQ(2u, manager_.GetLeastUnackedFecPacket());

  // Ack 2.
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 2;
  manager_.OnIncomingAck(received_info, false);

  EXPECT_EQ(3u, manager_.GetLeastUnackedFecPacket());

  // Discard the 3rd packet and ensure there are no FEC packets.
  manager_.DiscardFecPacket(3);
  EXPECT_FALSE(manager_.HasUnackedFecPackets());
}

TEST_P(QuicSentPacketManagerTest, GetFecSentTime) {
  VerifyUnackedPackets(NULL, 0);

  SerializedPacket serialized_packet(CreateFecPacket(1));
  manager_.OnSerializedPacket(serialized_packet, QuicTime::Zero());
  SerializedPacket serialized_packet2(CreateFecPacket(2));
  QuicTime sent_time = QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(1));
  manager_.OnSerializedPacket(serialized_packet2, sent_time);

  VerifyUnackedPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);

  EXPECT_TRUE(manager_.HasUnackedFecPackets());
  EXPECT_EQ(QuicTime::Zero(), manager_.GetFecSentTime(1));
  EXPECT_EQ(sent_time, manager_.GetFecSentTime(2));
}

}  // namespace
}  // namespace test
}  // namespace net
