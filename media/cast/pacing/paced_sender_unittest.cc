// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/pacing/mock_packet_sender.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint8 kValue = 123;
static const size_t kSize1 = 100;
static const size_t kSize2 = 101;
static const size_t kSize3 = 102;
static const size_t kSize4 = 103;
static const size_t kNackSize = 104;
static const int64 kStartMillisecond = 123456789;

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest() {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual ~PacedSenderTest() {}

  virtual void SetUp() {
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_thread_ = new CastThread(task_runner_, task_runner_, task_runner_,
                                  task_runner_, task_runner_);
    paced_sender_.reset(new PacedSender(cast_thread_, &mock_transport_));
    paced_sender_->set_clock(&testing_clock_);
  }

  base::SimpleTestTickClock testing_clock_;
  MockPacketSender mock_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_ptr<PacedSender> paced_sender_;
  scoped_refptr<CastThread> cast_thread_;
};

TEST_F(PacedSenderTest, PassThroughRtcp) {
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(1).WillRepeatedly(
      testing::Return(true));

  std::vector<uint8> packet(kSize1, kValue);
  int num_of_packets = 1;
  EXPECT_TRUE(paced_sender_->SendPacket(packet, num_of_packets));

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(0);
  EXPECT_TRUE(paced_sender_->ResendPacket(packet, num_of_packets));

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(1).WillRepeatedly(
      testing::Return(true));
  EXPECT_TRUE(paced_sender_->SendRtcpPacket(packet));
}

TEST_F(PacedSenderTest, BasicPace) {
  std::vector<uint8> packet(kSize1, kValue);
  int num_of_packets = 9;

  EXPECT_CALL(mock_transport_,
      SendPacket(_, kSize1)).Times(3).WillRepeatedly(testing::Return(true));
  for (int i = 0; i < num_of_packets; ++i) {
    EXPECT_TRUE(paced_sender_->SendPacket(packet, num_of_packets));
  }
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(10);

  // Check that we get the next burst.
  EXPECT_CALL(mock_transport_,
      SendPacket(_, kSize1)).Times(3).WillRepeatedly(testing::Return(true));

  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // If we call process too early make sure we don't send any packets.
  timeout = base::TimeDelta::FromMilliseconds(5);
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(0);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we get the next burst.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3).WillRepeatedly(
      testing::Return(true));
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we don't get any more packets.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(0);
  timeout = base::TimeDelta::FromMilliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();
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
    EXPECT_TRUE(paced_sender_->SendPacket(firts_packet,
                                          num_of_packets_in_frame));
  }
  // Add first NACK request.
  for (int i = 0; i < num_of_packets_in_nack; ++i) {
    EXPECT_TRUE(paced_sender_->ResendPacket(nack_packet,
                                            num_of_packets_in_nack));
  }
  // Check that we get the first NACK burst.
  EXPECT_CALL(mock_transport_, SendPacket(_, kNackSize)).Times(5).
      WillRepeatedly(testing::Return(true));

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second NACK request.
  for (int i = 0; i < num_of_packets_in_nack; ++i) {
    EXPECT_TRUE(paced_sender_->ResendPacket(nack_packet,
                                           num_of_packets_in_nack));
  }

  // Check that we get the next NACK burst.
  EXPECT_CALL(mock_transport_, SendPacket(_, kNackSize)).Times(7)
      .WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // End of NACK plus a packet from the oldest frame.
  EXPECT_CALL(mock_transport_, SendPacket(_, kNackSize)).Times(6)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(1)
      .WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second frame.
  // Make sure we don't delay the second frame due to the previous packets.
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_->SendPacket(second_packet,
                                         num_of_packets_in_frame));
  }

  // Last packets of frame 1 and the first packets of frame 2.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(5).WillRepeatedly(
      testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(2).WillRepeatedly(
      testing::Return(true));
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Last packets of frame 2.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(7).WillRepeatedly(
      testing::Return(true));
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // No more packets.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(0);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();
}

TEST_F(PacedSenderTest, PaceWith60fps) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  std::vector<uint8> firts_packet(kSize1, kValue);
  std::vector<uint8> second_packet(kSize2, kValue);
  std::vector<uint8> third_packet(kSize3, kValue);
  std::vector<uint8> fourth_packet(kSize4, kValue);
  base::TimeDelta timeout_10ms = base::TimeDelta::FromMilliseconds(10);
  int num_of_packets_in_frame = 9;

  // Check that the first burst of the frame go out on the wire.
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3).WillRepeatedly(
      testing::Return(true));
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_->SendPacket(firts_packet,
                                          num_of_packets_in_frame));
  }

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3).
      WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(6));

  // Add second frame, after 16 ms.
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_->SendPacket(second_packet,
                                         num_of_packets_in_frame));
  }
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(4));

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize1)).Times(3)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(1)
      .WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(4)
      .WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(3));

  // Add third frame, after 33 ms.
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_->SendPacket(third_packet,
                                         num_of_packets_in_frame));
  }
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize2)).Times(4)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize3)).Times(1)
      .WillRepeatedly(testing::Return(true));

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(7));
  task_runner_->RunTasks();

  // Add fourth frame, after 50 ms.
  for (int i = 0; i < num_of_packets_in_frame; ++i) {
    EXPECT_TRUE(paced_sender_->SendPacket(fourth_packet,
                                         num_of_packets_in_frame));
  }

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize3)).Times(6)
      .WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize3)).Times(2)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_transport_, SendPacket(_, kSize4)).Times(4)
      .WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize4)).Times(5)
      .WillRepeatedly(testing::Return(true));
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  EXPECT_CALL(mock_transport_, SendPacket(_, kSize4)).Times(0);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media
