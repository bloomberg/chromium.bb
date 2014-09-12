// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/big_endian.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/logging/logging_impl.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace media {
namespace cast {
namespace {

static const uint8 kValue = 123;
static const size_t kSize1 = 101;
static const size_t kSize2 = 102;
static const size_t kSize3 = 103;
static const size_t kSize4 = 104;
static const size_t kNackSize = 105;
static const int64 kStartMillisecond = INT64_C(12345678900000);
static const uint32 kVideoSsrc = 0x1234;
static const uint32 kAudioSsrc = 0x5678;

class TestPacketSender : public PacketSender {
 public:
  TestPacketSender() : bytes_sent_(0) {}

  virtual bool SendPacket(PacketRef packet, const base::Closure& cb) OVERRIDE {
    EXPECT_FALSE(expected_packet_size_.empty());
    size_t expected_packet_size = expected_packet_size_.front();
    expected_packet_size_.pop_front();
    EXPECT_EQ(expected_packet_size, packet->data.size());
    bytes_sent_ += packet->data.size();
    return true;
  }

  virtual int64 GetBytesSent() OVERRIDE {
    return bytes_sent_;
  }

  void AddExpectedSize(int expected_packet_size, int repeat_count) {
    for (int i = 0; i < repeat_count; ++i) {
      expected_packet_size_.push_back(expected_packet_size);
    }
  }

 public:
  std::list<int> expected_packet_size_;
  int64 bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketSender);
};

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest() {
    logging_.AddRawEventSubscriber(&subscriber_);
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(&testing_clock_);
    paced_sender_.reset(new PacedSender(kTargetBurstSize,
                                        kMaxBurstSize,
                                        &testing_clock_,
                                        &logging_,
                                        &mock_transport_,
                                        task_runner_));
    paced_sender_->RegisterAudioSsrc(kAudioSsrc);
    paced_sender_->RegisterVideoSsrc(kVideoSsrc);
  }

  virtual ~PacedSenderTest() {
    logging_.RemoveRawEventSubscriber(&subscriber_);
  }

  static void UpdateCastTransportStatus(CastTransportStatus status) {
    NOTREACHED();
  }

  SendPacketVector CreateSendPacketVector(size_t packet_size,
                                          int num_of_packets_in_frame,
                                          bool audio) {
    DCHECK_GE(packet_size, 12u);
    SendPacketVector packets;
    base::TimeTicks frame_tick = testing_clock_.NowTicks();
    // Advance the clock so that we don't get the same frame_tick
    // next time this function is called.
    testing_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
    for (int i = 0; i < num_of_packets_in_frame; ++i) {
      PacketKey key = PacedPacketSender::MakePacketKey(
          frame_tick,
          audio ? kAudioSsrc : kVideoSsrc, // ssrc
          i);

      PacketRef packet(new base::RefCountedData<Packet>);
      packet->data.resize(packet_size, kValue);
      // Write ssrc to packet so that it can be recognized as a
      // "video frame" for logging purposes.
      base::BigEndianWriter writer(
          reinterpret_cast<char*>(&packet->data[8]), 4);
      bool success = writer.WriteU32(audio ? kAudioSsrc : kVideoSsrc);
      DCHECK(success);
      packets.push_back(std::make_pair(key, packet));
    }
    return packets;
  }

  // Use this function to drain the packet list in PacedSender without having
  // to test the pacing implementation details.
  bool RunUntilEmpty(int max_tries) {
    for (int i = 0; i < max_tries; i++) {
      testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
      task_runner_->RunTasks();
      if (mock_transport_.expected_packet_size_.empty())
        return true;
      i++;
    }

    return mock_transport_.expected_packet_size_.empty();
  }

  LoggingImpl logging_;
  SimpleEventSubscriber subscriber_;
  base::SimpleTestTickClock testing_clock_;
  TestPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<PacedSender> paced_sender_;

  DISALLOW_COPY_AND_ASSIGN(PacedSenderTest);
};

}  // namespace

TEST_F(PacedSenderTest, PassThroughRtcp) {
  mock_transport_.AddExpectedSize(kSize1, 2);
  SendPacketVector packets = CreateSendPacketVector(kSize1, 1, true);

  EXPECT_TRUE(paced_sender_->SendPackets(packets));
  EXPECT_TRUE(paced_sender_->ResendPackets(packets, DedupInfo()));

  mock_transport_.AddExpectedSize(kSize2, 1);
  Packet tmp(kSize2, kValue);
  EXPECT_TRUE(paced_sender_->SendRtcpPacket(
      1,
      new base::RefCountedData<Packet>(tmp)));
}

TEST_F(PacedSenderTest, BasicPace) {
  int num_of_packets = 27;
  SendPacketVector packets = CreateSendPacketVector(kSize1,
                                                    num_of_packets,
                                                    false);

  mock_transport_.AddExpectedSize(kSize1, 10);
  EXPECT_TRUE(paced_sender_->SendPackets(packets));

  // Check that we get the next burst.
  mock_transport_.AddExpectedSize(kSize1, 10);

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // If we call process too early make sure we don't send any packets.
  timeout = base::TimeDelta::FromMilliseconds(5);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we get the next burst.
  mock_transport_.AddExpectedSize(kSize1, 7);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we don't get any more packets.
  EXPECT_TRUE(RunUntilEmpty(3));

  std::vector<PacketEvent> packet_events;
  subscriber_.GetPacketEventsAndReset(&packet_events);
  EXPECT_EQ(num_of_packets, static_cast<int>(packet_events.size()));
  int sent_to_network_event_count = 0;
  for (std::vector<PacketEvent>::iterator it = packet_events.begin();
       it != packet_events.end();
       ++it) {
    if (it->type == PACKET_SENT_TO_NETWORK)
      sent_to_network_event_count++;
    else
      FAIL() << "Got unexpected event type " << CastLoggingToString(it->type);
  }
  EXPECT_EQ(num_of_packets, sent_to_network_event_count);
}

TEST_F(PacedSenderTest, PaceWithNack) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  int num_of_packets_in_frame = 12;
  int num_of_packets_in_nack = 12;

  SendPacketVector nack_packets =
      CreateSendPacketVector(kNackSize, num_of_packets_in_nack, false);

  SendPacketVector first_frame_packets =
      CreateSendPacketVector(kSize1, num_of_packets_in_frame, false);

  SendPacketVector second_frame_packets =
      CreateSendPacketVector(kSize2, num_of_packets_in_frame, true);

  // Check that the first burst of the frame go out on the wire.
  mock_transport_.AddExpectedSize(kSize1, 10);
  EXPECT_TRUE(paced_sender_->SendPackets(first_frame_packets));

  // Add first NACK request.
  EXPECT_TRUE(paced_sender_->ResendPackets(nack_packets, DedupInfo()));

  // Check that we get the first NACK burst.
  mock_transport_.AddExpectedSize(kNackSize, 10);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second NACK request.
  EXPECT_TRUE(paced_sender_->ResendPackets(nack_packets, DedupInfo()));

  // Check that we get the next NACK burst.
  mock_transport_.AddExpectedSize(kNackSize, 10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // End of NACK plus two packets from the oldest frame.
  // Note that two of the NACKs have been de-duped.
  mock_transport_.AddExpectedSize(kNackSize, 2);
  mock_transport_.AddExpectedSize(kSize1, 2);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second frame.
  // Make sure we don't delay the second frame due to the previous packets.
  mock_transport_.AddExpectedSize(kSize2, 10);
  EXPECT_TRUE(paced_sender_->SendPackets(second_frame_packets));

  // Last packets of frame 2.
  mock_transport_.AddExpectedSize(kSize2, 2);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // No more packets.
  EXPECT_TRUE(RunUntilEmpty(5));

  std::vector<PacketEvent> packet_events;
  subscriber_.GetPacketEventsAndReset(&packet_events);
  int expected_video_network_event_count = num_of_packets_in_frame;
  int expected_video_retransmitted_event_count = 2 * num_of_packets_in_nack;
  expected_video_retransmitted_event_count -= 2; // 2 packets deduped
  int expected_audio_network_event_count = num_of_packets_in_frame;
  EXPECT_EQ(expected_video_network_event_count +
            expected_video_retransmitted_event_count +
            expected_audio_network_event_count,
            static_cast<int>(packet_events.size()));
  int audio_network_event_count = 0;
  int video_network_event_count = 0;
  int video_retransmitted_event_count = 0;
  for (std::vector<PacketEvent>::iterator it = packet_events.begin();
       it != packet_events.end();
       ++it) {
    if (it->type == PACKET_SENT_TO_NETWORK) {
      if (it->media_type == VIDEO_EVENT)
        video_network_event_count++;
      else
        audio_network_event_count++;
    } else if (it->type == PACKET_RETRANSMITTED) {
      if (it->media_type == VIDEO_EVENT)
        video_retransmitted_event_count++;
    } else {
      FAIL() << "Got unexpected event type " << CastLoggingToString(it->type);
    }
  }
  EXPECT_EQ(expected_audio_network_event_count, audio_network_event_count);
  EXPECT_EQ(expected_video_network_event_count, video_network_event_count);
  EXPECT_EQ(expected_video_retransmitted_event_count,
            video_retransmitted_event_count);
}

TEST_F(PacedSenderTest, PaceWith60fps) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  int num_of_packets_in_frame = 17;

  SendPacketVector first_frame_packets =
      CreateSendPacketVector(kSize1, num_of_packets_in_frame, false);

  SendPacketVector second_frame_packets =
      CreateSendPacketVector(kSize2, num_of_packets_in_frame, false);

  SendPacketVector third_frame_packets =
      CreateSendPacketVector(kSize3, num_of_packets_in_frame, false);

  SendPacketVector fourth_frame_packets =
      CreateSendPacketVector(kSize4, num_of_packets_in_frame, false);

  base::TimeDelta timeout_10ms = base::TimeDelta::FromMilliseconds(10);

  // Check that the first burst of the frame go out on the wire.
  mock_transport_.AddExpectedSize(kSize1, 10);
  EXPECT_TRUE(paced_sender_->SendPackets(first_frame_packets));

  mock_transport_.AddExpectedSize(kSize1, 7);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(6));

  // Add second frame, after 16 ms.
  mock_transport_.AddExpectedSize(kSize2, 3);
  EXPECT_TRUE(paced_sender_->SendPackets(second_frame_packets));
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(4));

  mock_transport_.AddExpectedSize(kSize2, 10);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  mock_transport_.AddExpectedSize(kSize2, 4);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(3));

  // Add third frame, after 33 ms.
  mock_transport_.AddExpectedSize(kSize3, 6);
  EXPECT_TRUE(paced_sender_->SendPackets(third_frame_packets));

  mock_transport_.AddExpectedSize(kSize3, 10);
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(7));
  task_runner_->RunTasks();

  // Add fourth frame, after 50 ms.
  EXPECT_TRUE(paced_sender_->SendPackets(fourth_frame_packets));

  mock_transport_.AddExpectedSize(kSize3, 1);
  mock_transport_.AddExpectedSize(kSize4, 9);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  mock_transport_.AddExpectedSize(kSize4, 8);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  // No more packets.
  EXPECT_TRUE(RunUntilEmpty(5));
}

TEST_F(PacedSenderTest, SendPriority) {
  // Actual order to the network is:
  // 1. Video packets x 10.
  // 2. RTCP packet x 1.
  // 3. Audio packet x 1.
  // 4. Video retransmission packet x 10.
  // 5. Video packet x 10.
  mock_transport_.AddExpectedSize(kSize2, 10);  // Normal video packets.
  mock_transport_.AddExpectedSize(kSize3, 1);  // RTCP packet.
  mock_transport_.AddExpectedSize(kSize1, 1);  // Audio packet.
  mock_transport_.AddExpectedSize(kSize4, 10);  // Resend video packets.
  mock_transport_.AddExpectedSize(kSize2, 10);  // Normal video packets.

  paced_sender_->RegisterPrioritySsrc(kAudioSsrc);

  // Retransmission packets with the earlier timestamp.
  SendPacketVector resend_packets =
      CreateSendPacketVector(kSize4, 10, false);
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));

  // Send 20 normal video packets. Only 10 will be sent in this
  // call, the rest will be sitting in the queue waiting for pacing.
  EXPECT_TRUE(paced_sender_->SendPackets(
      CreateSendPacketVector(kSize2, 20, false)));

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));

  // Send normal audio packet. This is queued and will be sent
  // earlier than video packets.
  EXPECT_TRUE(paced_sender_->SendPackets(
      CreateSendPacketVector(kSize1, 1, true)));

  // Send RTCP packet. This is queued and will be sent first.
  EXPECT_TRUE(paced_sender_->SendRtcpPacket(
      kVideoSsrc,
      new base::RefCountedData<Packet>(Packet(kSize3, kValue))));

  // Resend video packets. This is queued and will be sent
  // earlier than normal video packets.
  EXPECT_TRUE(paced_sender_->ResendPackets(resend_packets, DedupInfo()));

  // Roll the clock. Queued packets will be sent in this order:
  // 1. RTCP packet x 1.
  // 2. Audio packet x 1.
  // 3. Video retransmission packet x 10.
  // 4. Video packet x 10.
  task_runner_->RunTasks();
  EXPECT_TRUE(RunUntilEmpty(4));
}

TEST_F(PacedSenderTest, GetLastByteSent) {
  mock_transport_.AddExpectedSize(kSize1, 4);

  SendPacketVector packets1 = CreateSendPacketVector(kSize1, 1, true);
  SendPacketVector packets2 = CreateSendPacketVector(kSize1, 1, false);

  EXPECT_TRUE(paced_sender_->SendPackets(packets1));
  EXPECT_EQ(static_cast<int64>(kSize1),
            paced_sender_->GetLastByteSentForPacket(packets1[0].first));
  EXPECT_EQ(static_cast<int64>(kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(0, paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));

  EXPECT_TRUE(paced_sender_->SendPackets(packets2));
  EXPECT_EQ(static_cast<int64>(2 * kSize1),
            paced_sender_->GetLastByteSentForPacket(packets2[0].first));
  EXPECT_EQ(static_cast<int64>(kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(static_cast<int64>(2 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));

  EXPECT_TRUE(paced_sender_->ResendPackets(packets1, DedupInfo()));
  EXPECT_EQ(static_cast<int64>(3 * kSize1),
            paced_sender_->GetLastByteSentForPacket(packets1[0].first));
  EXPECT_EQ(static_cast<int64>(3 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(static_cast<int64>(2 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));

  EXPECT_TRUE(paced_sender_->ResendPackets(packets2, DedupInfo()));
  EXPECT_EQ(static_cast<int64>(4 * kSize1),
            paced_sender_->GetLastByteSentForPacket(packets2[0].first));
  EXPECT_EQ(static_cast<int64>(3 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(static_cast<int64>(4 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));
}

TEST_F(PacedSenderTest, DedupWithResendInterval) {
  mock_transport_.AddExpectedSize(kSize1, 2);

  SendPacketVector packets = CreateSendPacketVector(kSize1, 1, true);
  EXPECT_TRUE(paced_sender_->SendPackets(packets));
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(10));

  DedupInfo dedup_info;
  dedup_info.resend_interval = base::TimeDelta::FromMilliseconds(20);

  // This packet will not be sent.
  EXPECT_TRUE(paced_sender_->ResendPackets(packets, dedup_info));
  EXPECT_EQ(static_cast<int64>(kSize1), mock_transport_.GetBytesSent());

  dedup_info.resend_interval = base::TimeDelta::FromMilliseconds(5);
  EXPECT_TRUE(paced_sender_->ResendPackets(packets, dedup_info));
  EXPECT_EQ(static_cast<int64>(2 * kSize1), mock_transport_.GetBytesSent());
}

}  // namespace cast
}  // namespace media
