// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/pacing/mock_packet_sender.h"
#include "media/cast/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace media {
namespace cast {

static const uint8 kValue = 123;
static const size_t kSize1 = 100;
static const size_t kSize2 = 101;
static const size_t kSize3 = 102;
static const size_t kSize4 = 103;
static const size_t kNackSize = 104;
static const int64 kStartMillisecond = 123456789;

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest()
      : paced_sender_(&mock_transport_) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    paced_sender_.set_clock(&testing_clock_);
  }

  virtual ~PacedSenderTest() {}

  MockPacketSender mock_transport_;
  PacedSender paced_sender_;
  base::SimpleTestTickClock testing_clock_;
};

TEST_F(PacedSenderTest, PassThroughRtcp) {
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(1).WillRepeatedly(
      testing::Return(true));

  std::vector<uint8> packet(kSize1, kValue);
  int num_of_packets = 1;
  EXPECT_TRUE(paced_sender_.SendPacket(packet, num_of_packets));

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(0);
  EXPECT_TRUE(paced_sender_.ResendPacket(packet, num_of_packets));

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(1).WillRepeatedly(
      testing::Return(true));
  EXPECT_TRUE(paced_sender_.SendRtcpPacket(packet));
}

TEST_F(PacedSenderTest, BasicPace) {
  std::vector<uint8> packet(kSize1, kValue);
  int num_of_packets = 9;

  EXPECT_CALL(mock_transport_,
      SendPacket(_, kSize1)).Times(3).WillRepeatedly(testing::Return(true));
  for (int i = 0; i < num_of_packets; ++i) {
    EXPECT_TRUE(paced_sender_.SendPacket(packet, num_of_packets));
  }
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));

  // Check that we get the next burst.
  EXPECT_CALL(mock_transport_,
      SendPacket(_, kSize1)).Times(3).WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  // If we call process too early make sure we don't send any packets.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(5));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(0);
  paced_sender_.Process();

  // Check that we get the next burst.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(5));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3).WillRepeatedly(
      testing::Return(true));
  paced_sender_.Process();

  // Check that we don't get any more packets.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(0);
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  paced_sender_.Process();
}

TEST_F(PacedSenderTest, PaceWithNack) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  std::vector<uint8> firts_packet(kSize1, kValue);
  std::vector<uint8> second_packet(kSize2, kValue);
  std::vector<uint8> nack_packet(kNackSize, kValue);
  int num_of_packets_in_frame = 9;
  int num_of_packets_in_nack = 9;

  // Check that the first burst of the frame go out on the wire.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3).WillRepeatedly(
      testing::Return(true));
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_.SendPacket(firts_packet,
                                          num_of_packets_in_frame));
  }
  // Add first NACK request.
  for (int i = 0; i < num_of_packets_in_nack; ++i) {
    EXPECT_TRUE(paced_sender_.ResendPacket(nack_packet,
                                            num_of_packets_in_nack));
  }
  // Check that we get the first NACK burst.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kNackSize)).Times(5).
      WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  // Add second NACK request.
  for (int i = 0; i < num_of_packets_in_nack; ++i) {
    EXPECT_TRUE(paced_sender_.ResendPacket(nack_packet,
                                           num_of_packets_in_nack));
  }

  // Check that we get the next NACK burst.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kNackSize)).Times(7)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  // End of NACK plus a packet from the oldest frame.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kNackSize)).Times(6)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(1)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  // Add second frame.
  // Make sure we don't delay the second frame due to the previous packets.
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_.SendPacket(second_packet,
                                         num_of_packets_in_frame));
  }

  // Last packets of frame 1 and the first packets of frame 2.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(5).WillRepeatedly(
      testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(2).WillRepeatedly(
      testing::Return(true));
  paced_sender_.Process();

  // Last packets of frame 2.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(7).WillRepeatedly(
      testing::Return(true));
  paced_sender_.Process();

  // No more packets.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(0);
  paced_sender_.Process();
}

TEST_F(PacedSenderTest, PaceWith60fps) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  std::vector<uint8_t> firts_packet(kSize1, kValue);
  std::vector<uint8_t> second_packet(kSize2, kValue);
  std::vector<uint8_t> third_packet(kSize3, kValue);
  std::vector<uint8_t> fourth_packet(kSize4, kValue);
  int num_of_packets_in_frame = 9;

  // Check that the first burst of the frame go out on the wire.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3).WillRepeatedly(
      testing::Return(true));
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_.SendPacket(firts_packet,
                                         num_of_packets_in_frame));
  }

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3).
      WillRepeatedly(testing::Return(true));
  paced_sender_.Process();
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(6));

  // Add second frame, after 16 ms.
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_.SendPacket(second_packet,
                                         num_of_packets_in_frame));
  }

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(4));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(1)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(4)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(3));

  // Add third frame, after 33 ms.
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_.SendPacket(third_packet,
                                         num_of_packets_in_frame));
  }

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(7));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(4)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize3)).Times(1)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  // Add fourth frame, after 50 ms.
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_.SendPacket(fourth_packet,
                                         num_of_packets_in_frame));
  }

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize3)).Times(6)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize3)).Times(2)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize4)).Times(4)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize4)).Times(5)
      .WillRepeatedly(testing::Return(true));
  paced_sender_.Process();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize4)).Times(0);
  paced_sender_.Process();
}

}  // namespace cast
}  // namespace media
