// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {
namespace transport {

using testing::_;

static const uint8 kValue = 123;
static const size_t kSize1 = 100;
static const size_t kSize2 = 101;
static const size_t kSize3 = 102;
static const size_t kSize4 = 103;
static const size_t kNackSize = 104;
static const int64 kStartMillisecond = GG_INT64_C(12345678900000);

class TestPacketSender : public PacketSender {
 public:
  TestPacketSender() {}

  virtual bool SendPacket(const Packet& packet) OVERRIDE {
    EXPECT_FALSE(expected_packet_size_.empty());
    size_t expected_packet_size = expected_packet_size_.front();
    expected_packet_size_.pop_front();
    EXPECT_EQ(expected_packet_size, packet.size());
    return true;
  }

  void AddExpectedSize(int expected_packet_size, int repeat_count) {
    for (int i = 0; i < repeat_count; ++i) {
      expected_packet_size_.push_back(expected_packet_size);
    }
  }

 private:
  std::list<int> expected_packet_size_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketSender);
};

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest() {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(&testing_clock_);
    paced_sender_.reset(
        new PacedSender(&testing_clock_, &mock_transport_, task_runner_));
  }

  virtual ~PacedSenderTest() {}

  static void UpdateCastTransportStatus(transport::CastTransportStatus status) {
    NOTREACHED();
  }

  PacketList CreatePacketList(size_t packet_size, int num_of_packets_in_frame) {
    PacketList packets;
    for (int i = 0; i < num_of_packets_in_frame; ++i) {
      packets.push_back(Packet(packet_size, kValue));
    }
    return packets;
  }

  base::SimpleTestTickClock testing_clock_;
  TestPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<PacedSender> paced_sender_;

  DISALLOW_COPY_AND_ASSIGN(PacedSenderTest);
};

TEST_F(PacedSenderTest, PassThroughRtcp) {
  mock_transport_.AddExpectedSize(kSize1, 1);
  PacketList packets = CreatePacketList(kSize1, 1);

  EXPECT_TRUE(paced_sender_->SendPackets(packets));
  EXPECT_TRUE(paced_sender_->ResendPackets(packets));

  mock_transport_.AddExpectedSize(kSize2, 1);
  EXPECT_TRUE(paced_sender_->SendRtcpPacket(Packet(kSize2, kValue)));
}

TEST_F(PacedSenderTest, BasicPace) {
  int num_of_packets = 9;
  PacketList packets = CreatePacketList(kSize1, num_of_packets);

  mock_transport_.AddExpectedSize(kSize1, 3);
  EXPECT_TRUE(paced_sender_->SendPackets(packets));

  // Check that we get the next burst.
  mock_transport_.AddExpectedSize(kSize1, 3);

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // If we call process too early make sure we don't send any packets.
  timeout = base::TimeDelta::FromMilliseconds(5);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we get the next burst.
  mock_transport_.AddExpectedSize(kSize1, 3);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we don't get any more packets.
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();
}

TEST_F(PacedSenderTest, PaceWithNack) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  int num_of_packets_in_frame = 9;
  int num_of_packets_in_nack = 9;

  PacketList first_frame_packets =
      CreatePacketList(kSize1, num_of_packets_in_frame);

  PacketList second_frame_packets =
      CreatePacketList(kSize2, num_of_packets_in_frame);

  PacketList nack_packets = CreatePacketList(kNackSize, num_of_packets_in_nack);

  // Check that the first burst of the frame go out on the wire.
  mock_transport_.AddExpectedSize(kSize1, 3);
  EXPECT_TRUE(paced_sender_->SendPackets(first_frame_packets));

  // Add first NACK request.
  EXPECT_TRUE(paced_sender_->ResendPackets(nack_packets));

  // Check that we get the first NACK burst.
  mock_transport_.AddExpectedSize(kNackSize, 5);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second NACK request.
  EXPECT_TRUE(paced_sender_->ResendPackets(nack_packets));

  // Check that we get the next NACK burst.
  mock_transport_.AddExpectedSize(kNackSize, 7);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // End of NACK plus a packet from the oldest frame.
  mock_transport_.AddExpectedSize(kNackSize, 6);
  mock_transport_.AddExpectedSize(kSize1, 1);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second frame.
  // Make sure we don't delay the second frame due to the previous packets.
  EXPECT_TRUE(paced_sender_->SendPackets(second_frame_packets));

  // Last packets of frame 1 and the first packets of frame 2.
  mock_transport_.AddExpectedSize(kSize1, 5);
  mock_transport_.AddExpectedSize(kSize2, 2);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Last packets of frame 2.
  mock_transport_.AddExpectedSize(kSize2, 7);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // No more packets.
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();
}

TEST_F(PacedSenderTest, PaceWith60fps) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  int num_of_packets_in_frame = 9;

  PacketList first_frame_packets =
      CreatePacketList(kSize1, num_of_packets_in_frame);

  PacketList second_frame_packets =
      CreatePacketList(kSize2, num_of_packets_in_frame);

  PacketList third_frame_packets =
      CreatePacketList(kSize3, num_of_packets_in_frame);

  PacketList fourth_frame_packets =
      CreatePacketList(kSize4, num_of_packets_in_frame);

  base::TimeDelta timeout_10ms = base::TimeDelta::FromMilliseconds(10);

  // Check that the first burst of the frame go out on the wire.
  mock_transport_.AddExpectedSize(kSize1, 3);
  EXPECT_TRUE(paced_sender_->SendPackets(first_frame_packets));

  mock_transport_.AddExpectedSize(kSize1, 3);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(6));

  // Add second frame, after 16 ms.
  EXPECT_TRUE(paced_sender_->SendPackets(second_frame_packets));
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(4));

  mock_transport_.AddExpectedSize(kSize1, 3);
  mock_transport_.AddExpectedSize(kSize2, 1);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  mock_transport_.AddExpectedSize(kSize2, 4);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(3));

  // Add third frame, after 33 ms.
  EXPECT_TRUE(paced_sender_->SendPackets(third_frame_packets));
  mock_transport_.AddExpectedSize(kSize2, 4);
  mock_transport_.AddExpectedSize(kSize3, 1);

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(7));
  task_runner_->RunTasks();

  // Add fourth frame, after 50 ms.
  EXPECT_TRUE(paced_sender_->SendPackets(fourth_frame_packets));

  mock_transport_.AddExpectedSize(kSize3, 6);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  mock_transport_.AddExpectedSize(kSize3, 2);
  mock_transport_.AddExpectedSize(kSize4, 4);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  mock_transport_.AddExpectedSize(kSize4, 5);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();
}

}  // namespace transport
}  // namespace cast
}  // namespace media
