// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/rtcp/rtcp_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {
static const uint32 kSendingSsrc = 0x12345678;
static const uint32 kMediaSsrc = 0x87654321;
static const std::string kCName("test@10.1.1.1");
}  // namespace

class TestRtcpTransport : public PacedPacketSender {
 public:
  TestRtcpTransport()
      : expected_packet_length_(0),
        packet_count_(0) {
  }

  virtual bool SendRtcpPacket(const Packet& packet) OVERRIDE {
    EXPECT_EQ(expected_packet_length_, packet.size());
    EXPECT_EQ(0, memcmp(expected_packet_, &(packet[0]), packet.size()));
    packet_count_++;
    return true;
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  virtual bool ResendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  void SetExpectedRtcpPacket(const uint8* rtcp_buffer, size_t length) {
    expected_packet_length_ = length;
    memcpy(expected_packet_, rtcp_buffer, length);
  }

  int packet_count() const { return packet_count_; }

 private:
  uint8 expected_packet_[kIpPacketSize];
  size_t expected_packet_length_;
  int packet_count_;
};

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest()
      : task_runner_(new test::FakeTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_, task_runner_,
            task_runner_, task_runner_, task_runner_, task_runner_,
            GetDefaultCastLoggingConfig())),
        rtcp_sender_(new RtcpSender(cast_environment_,
                                    &test_transport_,
                                    kSendingSsrc,
                                    kCName)) {
  }

  base::SimpleTestTickClock testing_clock_;
  TestRtcpTransport test_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<RtcpSender> rtcp_sender_;
};

TEST_F(RtcpSenderTest, RtcpSenderReport) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report + c_name.
  TestRtcpPacketBuilder p;
  p.AddSr(kSendingSsrc, 0);
  p.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  rtcp_sender_->SendRtcpFromRtpSender(RtcpSender::kRtcpSr,
                                      &sender_info,
                                      NULL,
                                      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReport) {
  // Empty receiver report + c_name.
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSendingSsrc, 0);
  p1.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p1.Packet(), p1.Length());

  rtcp_sender_->SendRtcpFromRtpReceiver(RtcpSender::kRtcpRr,
      NULL, NULL, NULL, NULL);

  EXPECT_EQ(1, test_transport_.packet_count());

  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p2;
  p2.AddRr(kSendingSsrc, 1);
  p2.AddRb(kMediaSsrc);
  p2.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p2.Packet(), p2.Length());

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  rtcp_sender_->SendRtcpFromRtpReceiver(RtcpSender::kRtcpRr, &report_block,
                                        NULL, NULL, NULL);

  EXPECT_EQ(2, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpSenderReportWithDlrr) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report + c_name + dlrr.
  TestRtcpPacketBuilder p1;
  p1.AddSr(kSendingSsrc, 0);
  p1.AddSdesCname(kSendingSsrc, kCName);
  p1.AddXrHeader(kSendingSsrc);
  p1.AddXrDlrrBlock(kSendingSsrc);
  test_transport_.SetExpectedRtcpPacket(p1.Packet(), p1.Length());

  RtcpDlrrReportBlock dlrr_rb;
  dlrr_rb.last_rr = kLastRr;
  dlrr_rb.delay_since_last_rr = kDelayLastRr;

  rtcp_sender_->SendRtcpFromRtpSender(
      RtcpSender::kRtcpSr | RtcpSender::kRtcpDlrr,
      &sender_info,
      &dlrr_rb,
      NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpSenderReportWithDlrrAndLog) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report + c_name + dlrr + sender log.
  TestRtcpPacketBuilder p;
  p.AddSr(kSendingSsrc, 0);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrDlrrBlock(kSendingSsrc);
  p.AddSenderLog(kSendingSsrc);
  p.AddSenderFrameLog(kRtcpSenderFrameStatusSentToNetwork, kRtpTimestamp);

  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpDlrrReportBlock dlrr_rb;
  dlrr_rb.last_rr = kLastRr;
  dlrr_rb.delay_since_last_rr = kDelayLastRr;

  RtcpSenderFrameLogMessage sender_frame_log;
  sender_frame_log.frame_status = kRtcpSenderFrameStatusSentToNetwork;
  sender_frame_log.rtp_timestamp = kRtpTimestamp;

  RtcpSenderLogMessage sender_log;
  sender_log.push_back(sender_frame_log);

  rtcp_sender_->SendRtcpFromRtpSender(
      RtcpSender::kRtcpSr | RtcpSender::kRtcpDlrr | RtcpSender::kRtcpSenderLog,
      &sender_info,
      &dlrr_rb,
      &sender_log);

  EXPECT_EQ(1, test_transport_.packet_count());
  EXPECT_TRUE(sender_log.empty());
}

TEST_F(RtcpSenderTest, RtcpSenderReporWithTooManyLogFrames) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report + c_name + sender log.
  TestRtcpPacketBuilder p;
  p.AddSr(kSendingSsrc, 0);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddSenderLog(kSendingSsrc);

  for (int i = 0; i < 359; ++i) {
    p.AddSenderFrameLog(kRtcpSenderFrameStatusSentToNetwork,
                        kRtpTimestamp + i * 90);
  }
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());


  RtcpSenderLogMessage sender_log;
  for (int j = 0; j < 400; ++j) {
    RtcpSenderFrameLogMessage sender_frame_log;
    sender_frame_log.frame_status = kRtcpSenderFrameStatusSentToNetwork;
    sender_frame_log.rtp_timestamp = kRtpTimestamp + j * 90;
    sender_log.push_back(sender_frame_log);
  }

  rtcp_sender_->SendRtcpFromRtpSender(
      RtcpSender::kRtcpSr | RtcpSender::kRtcpSenderLog,
      &sender_info,
      NULL,
      &sender_log);

  EXPECT_EQ(1, test_transport_.packet_count());
  EXPECT_EQ(41u, sender_log.size());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithRrtr) {
  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

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
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

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
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

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
  static const uint32 kDelayDeltaMs = 123;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrRrtrBlock();
  p.AddCast(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

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

  // Test empty Log message.
  RtcpReceiverLogMessage receiver_log;

  VLOG(0) << " Test empty Log  " ;
  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast |
      RtcpSender::kRtcpReceiverLog,
      &report_block,
      &rrtr,
      &cast_message,
      &receiver_log);


  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);
  p.AddReceiverFrameLog(kRtpTimestamp, 2, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, 1, 0);
  p.AddReceiverEventLog(kLostPacketId1, 6, kTimeDelayMs);

  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
  RtcpReceiverEventLogMessage event_log;

  event_log.type = kAckSent;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
  frame_log.event_log_messages_.push_back(event_log);

  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  event_log.type = kPacketReceived;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.packet_id = kLostPacketId1;
  frame_log.event_log_messages_.push_back(event_log);

  receiver_log.push_back(frame_log);

  VLOG(0) << " Test  Log  " ;
  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr | RtcpSender::kRtcpCast |
      RtcpSender::kRtcpReceiverLog,
      &report_block,
      &rrtr,
      &cast_message,
      &receiver_log);

  EXPECT_TRUE(receiver_log.empty());
  EXPECT_EQ(2, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithOversizedFrameLog) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, 1, 0);
  p.AddReceiverFrameLog(kRtpTimestamp + 2345,
      kRtcpMaxReceiverLogMessages, kTimeBaseMs);

  for (size_t i = 0; i < kRtcpMaxReceiverLogMessages; ++i) {
    p.AddReceiverEventLog(
        kLostPacketId1, 6, static_cast<uint16>(kTimeDelayMs * i));
  }

  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReceiverFrameLogMessage frame_1_log(kRtpTimestamp);
  RtcpReceiverEventLogMessage event_log;

  event_log.type = kAckSent;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
  frame_1_log.event_log_messages_.push_back(event_log);

  RtcpReceiverLogMessage receiver_log;
  receiver_log.push_back(frame_1_log);

  RtcpReceiverFrameLogMessage frame_2_log(kRtpTimestamp + 2345);

  for (int j = 0; j < 300; ++j) {
    event_log.type = kPacketReceived;
    event_log.event_timestamp = testing_clock.NowTicks();
    event_log.packet_id = kLostPacketId1;
    frame_2_log.event_log_messages_.push_back(event_log);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }
  receiver_log.push_back(frame_2_log);

  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpReceiverLog,
      &report_block,
      NULL,
      NULL,
      &receiver_log);

  EXPECT_EQ(1, test_transport_.packet_count());
  EXPECT_EQ(1u, receiver_log.size());
  EXPECT_EQ(300u - kRtcpMaxReceiverLogMessages,
            receiver_log.front().event_log_messages_.size());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithTooManyLogFrames) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;

  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number = kExtendedMax;
  report_block.jitter = kTestJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  p.AddReceiverLog(kSendingSsrc);

  for (int i = 0; i < 119; ++i) {
    p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs +  i * kTimeDelayMs);
    p.AddReceiverEventLog(kDelayDeltaMs, 1, 0);
  }
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReceiverLogMessage receiver_log;

  for (int j = 0; j < 200; ++j) {
    RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
    RtcpReceiverEventLogMessage event_log;

    event_log.type = kAckSent;
    event_log.event_timestamp = testing_clock.NowTicks();
    event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
    frame_log.event_log_messages_.push_back(event_log);
    receiver_log.push_back(frame_log);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }
  rtcp_sender_->SendRtcpFromRtpReceiver(
      RtcpSender::kRtcpRr | RtcpSender::kRtcpReceiverLog,
      &report_block,
      NULL,
      NULL,
      &receiver_log);

  EXPECT_EQ(1, test_transport_.packet_count());
  EXPECT_EQ(81u, receiver_log.size());
}

}  // namespace cast
}  // namespace media
