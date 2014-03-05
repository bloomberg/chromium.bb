// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {
static const uint32 kSendingSsrc = 0x12345678;
static const uint32 kMediaSsrc = 0x87654321;
static const std::string kCName("test@10.1.1.1");

transport::RtcpReportBlock GetReportBlock() {
  transport::RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;
  return report_block;
}

}  // namespace

class TestRtcpTransport : public transport::PacedPacketSender {
 public:
  TestRtcpTransport() : packet_count_(0) {}

  virtual bool SendRtcpPacket(const Packet& packet) OVERRIDE {
    EXPECT_EQ(expected_packet_.size(), packet.size());
    EXPECT_EQ(0, memcmp(expected_packet_.data(), packet.data(), packet.size()));
    packet_count_++;
    return true;
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE { return false; }

  virtual bool ResendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  void SetExpectedRtcpPacket(scoped_ptr<Packet> packet) {
    expected_packet_.swap(*packet);
  }

  int packet_count() const { return packet_count_; }

 private:
  Packet expected_packet_;
  int packet_count_;

  DISALLOW_COPY_AND_ASSIGN(TestRtcpTransport);
};

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            GetDefaultCastSenderLoggingConfig())),
        rtcp_sender_(new RtcpSender(cast_environment_,
                                    &test_transport_,
                                    kSendingSsrc,
                                    kCName)) {}

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  TestRtcpTransport test_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<RtcpSender> rtcp_sender_;

  DISALLOW_COPY_AND_ASSIGN(RtcpSenderTest);
};

TEST_F(RtcpSenderTest, RtcpReceiverReport) {
  // Empty receiver report + c_name.
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSendingSsrc, 0);
  p1.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p1.GetPacket());

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr, NULL, NULL, NULL, NULL);

  EXPECT_EQ(1, test_transport_.packet_count());

  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p2;
  p2.AddRr(kSendingSsrc, 1);
  p2.AddRb(kMediaSsrc);
  p2.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p2.GetPacket().Pass());

  transport::RtcpReportBlock report_block = GetReportBlock();

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr, &report_block, NULL, NULL, NULL);

  EXPECT_EQ(2, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithRrtr) {
  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block = GetReportBlock();

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr,
      &report_block,
      &rrtr,
      NULL,
      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithCast) {
  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddCast(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block = GetReportBlock();

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpCast,
      &report_block,
      NULL,
      &cast_message,
      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithRrtraAndCastMessage) {
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  p.AddCast(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block = GetReportBlock();

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast,
      &report_block,
      &rrtr,
      &cast_message,
      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithRrtrCastMessageAndLog) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  p.AddCast(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  transport::RtcpReportBlock report_block = GetReportBlock();

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;

  ReceiverRtcpEventSubscriber event_subscriber(
      500, ReceiverRtcpEventSubscriber::kVideoEventSubscriber);

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast |
          RtcpSender::kRtcpReceiverLog,
      &report_block,
      &rrtr,
      &cast_message,
      &event_subscriber);

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);
  p.AddReceiverFrameLog(kRtpTimestamp, 2, kTimeBaseMs);
  p.AddReceiverEventLog(0, 5, 0);
  p.AddReceiverEventLog(kLostPacketId1, 8, kTimeDelayMs);

  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  FrameEvent frame_event;
  frame_event.rtp_timestamp = kRtpTimestamp;
  frame_event.type = kVideoAckSent;
  frame_event.timestamp = testing_clock.NowTicks();
  event_subscriber.OnReceiveFrameEvent(frame_event);
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));

  PacketEvent packet_event;
  packet_event.rtp_timestamp = kRtpTimestamp;
  packet_event.type = kVideoPacketReceived;
  packet_event.timestamp = testing_clock.NowTicks();
  packet_event.packet_id = kLostPacketId1;
  event_subscriber.OnReceivePacketEvent(packet_event);
  EXPECT_EQ(2u, event_subscriber.get_rtcp_events().size());

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast |
          RtcpSender::kRtcpReceiverLog,
      &report_block,
      &rrtr,
      &cast_message,
      &event_subscriber);

  EXPECT_EQ(2, test_transport_.packet_count());

  // We expect to see the same packet because we send redundant events.
  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast |
          RtcpSender::kRtcpReceiverLog,
      &report_block,
      &rrtr,
      &cast_message,
      &event_subscriber);

  EXPECT_EQ(3, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithOversizedFrameLog) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);

  transport::RtcpReportBlock report_block = GetReportBlock();

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs);
  p.AddReceiverEventLog(0, 5, 0);
  p.AddReceiverFrameLog(
      kRtpTimestamp + 2345, kRtcpMaxReceiverLogMessages, kTimeBaseMs);

  for (size_t i = 0; i < kRtcpMaxReceiverLogMessages; ++i) {
    p.AddReceiverEventLog(
        kLostPacketId1, 8, static_cast<uint16>(kTimeDelayMs * i));
  }

  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  ReceiverRtcpEventSubscriber event_subscriber(
      500, ReceiverRtcpEventSubscriber::kVideoEventSubscriber);
  FrameEvent frame_event;
  frame_event.rtp_timestamp = kRtpTimestamp;
  frame_event.type = media::cast::kVideoAckSent;
  frame_event.timestamp = testing_clock.NowTicks();
  event_subscriber.OnReceiveFrameEvent(frame_event);

  for (size_t i = 0; i < kRtcpMaxReceiverLogMessages; ++i) {
    PacketEvent packet_event;
    packet_event.rtp_timestamp = kRtpTimestamp + 2345;
    packet_event.type = kVideoPacketReceived;
    packet_event.timestamp = testing_clock.NowTicks();
    packet_event.packet_id = kLostPacketId1;
    event_subscriber.OnReceivePacketEvent(packet_event);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpReceiverLog,
      &report_block,
      NULL,
      NULL,
      &event_subscriber);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithTooManyLogFrames) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);

  transport::RtcpReportBlock report_block = GetReportBlock();

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  // The last 119 events are sent.
  for (int i = kRtcpMaxReceiverLogMessages - 119;
       i < static_cast<int>(kRtcpMaxReceiverLogMessages);
       ++i) {
    p.AddReceiverFrameLog(kRtpTimestamp + i, 1, kTimeBaseMs + i * kTimeDelayMs);
    p.AddReceiverEventLog(0, 5, 0);
  }
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  ReceiverRtcpEventSubscriber event_subscriber(
      500, ReceiverRtcpEventSubscriber::kVideoEventSubscriber);
  for (size_t i = 0; i < kRtcpMaxReceiverLogMessages; ++i) {
    FrameEvent frame_event;
    frame_event.rtp_timestamp = kRtpTimestamp + static_cast<int>(i);
    frame_event.type = media::cast::kVideoAckSent;
    frame_event.timestamp = testing_clock.NowTicks();
    event_subscriber.OnReceiveFrameEvent(frame_event);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpReceiverLog,
      &report_block,
      NULL,
      NULL,
      &event_subscriber);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithOldLogFrames) {
  static const uint32 kTimeBaseMs = 12345678;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);

  transport::RtcpReportBlock report_block = GetReportBlock();

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  // Log 11 events for a single frame, each |kTimeBetweenEventsMs| apart.
  // Only last 10 events will be sent because the first event is more than
  // 4095 milliseconds away from latest event.
  const int kTimeBetweenEventsMs = 410;
  p.AddReceiverFrameLog(kRtpTimestamp, 10, kTimeBaseMs + kTimeBetweenEventsMs);
  for (int i = 0; i < 10; ++i) {
    p.AddReceiverEventLog(0, 5, i * kTimeBetweenEventsMs);
  }
  test_transport_.SetExpectedRtcpPacket(p.GetPacket().Pass());

  ReceiverRtcpEventSubscriber event_subscriber(
      500, ReceiverRtcpEventSubscriber::kVideoEventSubscriber);
  for (int i = 0; i < 11; ++i) {
    FrameEvent frame_event;
    frame_event.rtp_timestamp = kRtpTimestamp;
    frame_event.type = media::cast::kVideoAckSent;
    frame_event.timestamp = testing_clock.NowTicks();
    event_subscriber.OnReceiveFrameEvent(frame_event);
    testing_clock.Advance(
        base::TimeDelta::FromMilliseconds(kTimeBetweenEventsMs));
  }

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpReceiverLog,
      &report_block,
      NULL,
      NULL,
      &event_subscriber);

  EXPECT_EQ(1, test_transport_.packet_count());
}

}  // namespace cast
}  // namespace media
