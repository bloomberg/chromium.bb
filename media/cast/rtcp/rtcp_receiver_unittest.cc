// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/cast/rtcp/mock_rtcp_receiver_feedback.h"
#include "media/cast/rtcp/mock_rtcp_sender_feedback.h"
#include "media/cast/rtcp/rtcp_receiver.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint32 kSenderSsrc = 0x10203;
static const uint32 kSourceSsrc = 0x40506;
static const uint32 kUnknownSsrc = 0xDEAD;
static const std::string kCName("test@10.1.1.1");

namespace {
class SenderFeedbackCastVerification : public RtcpSenderFeedback {
 public:
  SenderFeedbackCastVerification() : called_(false) {}

  virtual void OnReceivedCastFeedback(
      const RtcpCastMessage& cast_feedback) OVERRIDE {
    EXPECT_EQ(cast_feedback.media_ssrc_, kSenderSsrc);
    EXPECT_EQ(cast_feedback.ack_frame_id_, kAckFrameId);

    MissingFramesAndPacketsMap::const_iterator frame_it =
        cast_feedback.missing_frames_and_packets_.begin();

    EXPECT_TRUE(frame_it != cast_feedback.missing_frames_and_packets_.end());
    EXPECT_EQ(kLostFrameId, frame_it->first);
    EXPECT_TRUE(frame_it->second.empty());
    ++frame_it;
    EXPECT_TRUE(frame_it != cast_feedback.missing_frames_and_packets_.end());
    EXPECT_EQ(kFrameIdWithLostPackets, frame_it->first);
    EXPECT_EQ(3UL, frame_it->second.size());
    PacketIdSet::const_iterator packet_it = frame_it->second.begin();
    EXPECT_EQ(kLostPacketId1, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId2, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId3, *packet_it);
    ++frame_it;
    EXPECT_EQ(frame_it, cast_feedback.missing_frames_and_packets_.end());
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  bool called_;
};
}  // namespace

class RtcpReceiverTest : public ::testing::Test {
 protected:
  RtcpReceiverTest()
      : rtcp_receiver_(new RtcpReceiver(&mock_sender_feedback_,
                                        &mock_receiver_feedback_,
                                        &mock_rtt_feedback_,
                                        kSourceSsrc)) {
  }

  virtual ~RtcpReceiverTest() {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(mock_receiver_feedback_, OnReceivedSenderReport(_)).Times(0);
    EXPECT_CALL(mock_receiver_feedback_,
                OnReceiverReferenceTimeReport(_)).Times(0);
    EXPECT_CALL(mock_receiver_feedback_,
                OnReceivedSendReportRequest()).Times(0);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(0);
    EXPECT_CALL(mock_rtt_feedback_,
                OnReceivedDelaySinceLastReport(_, _, _)).Times(0);

    expected_sender_info_.ntp_seconds = kNtpHigh;
    expected_sender_info_.ntp_fraction = kNtpLow;
    expected_sender_info_.rtp_timestamp = kRtpTimestamp;
    expected_sender_info_.send_packet_count = kSendPacketCount;
    expected_sender_info_.send_octet_count = kSendOctetCount;

    expected_report_block_.remote_ssrc = kSenderSsrc;
    expected_report_block_.media_ssrc = kSourceSsrc;
    expected_report_block_.fraction_lost = kLoss >> 24;
    expected_report_block_.cumulative_lost = kLoss & 0xffffff;
    expected_report_block_.extended_high_sequence_number = kExtendedMax;
    expected_report_block_.jitter = kJitter;
    expected_report_block_.last_sr = kLastSr;
    expected_report_block_.delay_since_last_sr = kDelayLastSr;
    expected_receiver_reference_report_.remote_ssrc = kSenderSsrc;
    expected_receiver_reference_report_.ntp_seconds = kNtpHigh;
    expected_receiver_reference_report_.ntp_fraction = kNtpLow;
  }

  // Injects an RTCP packet into the receiver.
  void InjectRtcpPacket(const uint8* packet, uint16 length) {
    RtcpParser rtcp_parser(packet, length);
    rtcp_receiver_->IncomingRtcpPacket(&rtcp_parser);
  }

  MockRtcpReceiverFeedback mock_receiver_feedback_;
  MockRtcpRttFeedback mock_rtt_feedback_;
  MockRtcpSenderFeedback mock_sender_feedback_;
  scoped_ptr<RtcpReceiver> rtcp_receiver_;
  RtcpSenderInfo expected_sender_info_;
  RtcpReportBlock expected_report_block_;
  RtcpReceiverReferenceTimeReport expected_receiver_reference_report_;
};

TEST_F(RtcpReceiverTest, BrokenPacketIsIgnored) {
  const uint8 bad_packet[] = {0, 0, 0, 0};
  InjectRtcpPacket(bad_packet, sizeof(bad_packet));
}

TEST_F(RtcpReceiverTest, InjectSenderReportPacket) {
  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);

  // Expected to be ignored since the sender ssrc does not match our
  // remote ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Expected to be pass through since the sender ssrc match our remote ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiveReportPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectSenderReportWithReportBlockPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddSr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_, OnReceivedSenderReport(_)).Times(0);
  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
        kLastSr,
        kDelayLastSr)).Times(1);

  rtcp_receiver_->SetRemoteSSRC(0);

  TestRtcpPacketBuilder p2;
  p2.AddSr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Receiver report expected to be pass through since the sender ssrc match
  // our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Receiver report expected to be pass through since the sender ssrc match
  // our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectSenderReportPacketWithDlrr) {
  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);
  p.AddXrHeader(kSenderSsrc);
  p.AddXrUnknownBlock();
  p.AddXrExtendedDlrrBlock(kSenderSsrc);
  p.AddXrUnknownBlock();
  p.AddSdesCname(kSenderSsrc, kCName);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSenderSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  // Enable receiving sender report.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithRrtr) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddXrHeader(kSenderSsrc);
  p1.AddXrRrtrBlock();

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);
  EXPECT_CALL(mock_receiver_feedback_, OnReceiverReferenceTimeReport(
      expected_receiver_reference_report_)).Times(1);

  // Enable receiving reference time report.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddXrHeader(kSenderSsrc);
  p2.AddXrRrtrBlock();

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithIntraFrameRequest) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddPli(kSenderSsrc, kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddPli(kSenderSsrc, kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithCastFeedback) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddCast(kSenderSsrc, kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);
  EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(1);

  // Enable receiving the cast feedback.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddCast(kSenderSsrc, kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithCastVerification) {
  SenderFeedbackCastVerification sender_feedback_cast_verification;
  RtcpReceiver rtcp_receiver(&sender_feedback_cast_verification,
                             &mock_receiver_feedback_,
                             &mock_rtt_feedback_,
                             kSourceSsrc);

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  // Enable receiving the cast feedback.
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddCast(kSenderSsrc, kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  RtcpParser rtcp_parser(p.Packet(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(sender_feedback_cast_verification.called());
}

}  // namespace cast
}  // namespace media
