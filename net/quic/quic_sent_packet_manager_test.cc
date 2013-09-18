// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_packet_manager.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace net {
namespace test {
namespace {

class MockHelper : public QuicSentPacketManager::HelperInterface {
 public:
  MOCK_METHOD0(GetPeerLargestObservedPacket, QuicPacketSequenceNumber());
  MOCK_METHOD0(GetNextPacketSequenceNumber, QuicPacketSequenceNumber());
  MOCK_METHOD2(OnPacketNacked, void(QuicPacketSequenceNumber sequence_number,
                                    size_t nack_count));
};

class QuicSentPacketManagerTest : public ::testing::Test {
 protected:
  QuicSentPacketManagerTest()
      : manager_(true, &helper_) {
  }

  testing::StrictMock<MockHelper> helper_;
  QuicSentPacketManager manager_;
};

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacket) {
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(1u));
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnacked) {
  scoped_ptr<QuicPacket> packet(QuicPacket::NewDataPacket(
      NULL, 0, false, PACKET_8BYTE_GUID, false, PACKET_6BYTE_SEQUENCE_NUMBER));
  SerializedPacket serialized_packet(1u, PACKET_6BYTE_SEQUENCE_NUMBER,
                                     packet.get(), 7u,
                                     new RetransmittableFrames());

  manager_.OnSerializedPacket(serialized_packet);
  EXPECT_EQ(1u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketUnackedFec) {
  scoped_ptr<QuicPacket> packet(QuicPacket::NewFecPacket(
      NULL, 0, false, PACKET_8BYTE_GUID, false, PACKET_6BYTE_SEQUENCE_NUMBER));
  SerializedPacket serialized_packet(1u, PACKET_6BYTE_SEQUENCE_NUMBER,
                                     packet.get(), 7u, NULL);

  manager_.OnSerializedPacket(serialized_packet);
  // FEC packets do not count as "unacked".
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(2u));
  EXPECT_EQ(2u, manager_.GetLeastUnackedSentPacket());
}

TEST_F(QuicSentPacketManagerTest, GetLeastUnackedSentPacketDiscardUnacked) {
  scoped_ptr<QuicPacket> packet(QuicPacket::NewDataPacket(
      NULL, 0, false, PACKET_8BYTE_GUID, false, PACKET_6BYTE_SEQUENCE_NUMBER));
  SerializedPacket serialized_packet(1u, PACKET_6BYTE_SEQUENCE_NUMBER,
                                     packet.get(), 7u,
                                     new RetransmittableFrames());

  manager_.OnSerializedPacket(serialized_packet);
  manager_.DiscardPacket(1u);
  EXPECT_CALL(helper_, GetNextPacketSequenceNumber()).WillOnce(Return(2u));
  EXPECT_EQ(2u, manager_.GetLeastUnackedSentPacket());
}

}  // namespace
}  // namespace test
}  // namespace net
