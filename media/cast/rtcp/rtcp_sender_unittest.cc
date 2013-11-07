// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_defines.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/rtcp/rtcp_sender.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

static const uint32 kSendingSsrc = 0x12345678;
static const uint32 kMediaSsrc = 0x87654321;
static const std::string kCName("test@10.1.1.1");

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
      : rtcp_sender_(new RtcpSender(&test_transport_,
                                    kSendingSsrc,
                                    kCName)) {
  }

  TestRtcpTransport test_transport_;
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

  rtcp_sender_->SendRtcp(RtcpSender::kRtcpSr,
                         &sender_info,
                         NULL,
                         0,
                         NULL,
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

  rtcp_sender_->SendRtcp(RtcpSender::kRtcpRr,
                         NULL,
                         NULL,
                         0,
                         NULL,
                         NULL,
                         NULL);

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
  report_block.extended_high_sequence_number =
      kExtendedMax;
  report_block.jitter = kJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  rtcp_sender_->SendRtcp(RtcpSender::kRtcpRr,
                         NULL,
                         &report_block,
                         0,
                         NULL,
                         NULL,
                         NULL);

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

  rtcp_sender_->SendRtcp(RtcpSender::kRtcpSr | RtcpSender::kRtcpDlrr,
                         &sender_info,
                         NULL,
                         0,
                         &dlrr_rb,
                         NULL,
                         NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
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
  report_block.extended_high_sequence_number =
      kExtendedMax;
  report_block.jitter = kJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = kNtpHigh;
  rrtr.ntp_fraction = kNtpLow;

  rtcp_sender_->SendRtcp(RtcpSender::kRtcpRr | RtcpSender::kRtcpRrtr,
                         NULL,
                         &report_block,
                         0,
                         NULL,
                         &rrtr,
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
  report_block.jitter = kJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  RtcpCastMessage cast_message(kMediaSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[
      kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;

  rtcp_sender_->SendRtcp(RtcpSender::kRtcpRr | RtcpSender::kRtcpCast,
                         NULL,
                         &report_block,
                         0,
                         NULL,
                         NULL,
                         &cast_message);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpSenderTest, RtcpReceiverReportWithIntraFrameRequest) {
  // Receiver report with report block + c_name.
  TestRtcpPacketBuilder p;
  p.AddRr(kSendingSsrc, 1);
  p.AddRb(kMediaSsrc);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddPli(kSendingSsrc, kMediaSsrc);
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpReportBlock report_block;
  // Initialize remote_ssrc to a "clearly illegal" value.
  report_block.remote_ssrc = 0xDEAD;
  report_block.media_ssrc = kMediaSsrc;  // SSRC of the RTP packet sender.
  report_block.fraction_lost = kLoss >> 24;
  report_block.cumulative_lost = kLoss;  // 24 bits valid.
  report_block.extended_high_sequence_number =
      kExtendedMax;
  report_block.jitter = kJitter;
  report_block.last_sr = kLastSr;
  report_block.delay_since_last_sr = kDelayLastSr;

  rtcp_sender_->SendRtcp(RtcpSender::kRtcpRr | RtcpSender::kRtcpPli,
                         NULL,
                         &report_block,
                         kMediaSsrc,
                         NULL,
                         NULL,
                         NULL);

  EXPECT_EQ(1, test_transport_.packet_count());
}

}  // namespace cast
}  // namespace media
