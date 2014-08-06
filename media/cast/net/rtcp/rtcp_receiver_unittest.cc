// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/mock_rtcp_receiver_feedback.h"
#include "media/cast/net/rtcp/rtcp_receiver.h"
#include "media/cast/net/rtcp/rtcp_utility.h"
#include "media/cast/net/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint32 kSenderSsrc = 0x10203;
static const uint32 kSourceSsrc = 0x40506;
static const uint32 kUnknownSsrc = 0xDEAD;
static const base::TimeDelta kTargetDelay =
    base::TimeDelta::FromMilliseconds(100);

namespace {

class RtcpMessageVerification : public MockRtcpReceiverFeedback {
 public:
  RtcpMessageVerification()
      : called_on_received_sender_log_(false),
        called_on_received_receiver_log_(false),
        called_on_received_cast_message_(false) {}

  virtual void OnReceivedReceiverLog(const RtcpReceiverLogMessage& receiver_log)
      OVERRIDE {
    EXPECT_EQ(expected_receiver_log_.size(), receiver_log.size());
    RtcpReceiverLogMessage::const_iterator expected_it =
        expected_receiver_log_.begin();
    RtcpReceiverLogMessage::const_iterator incoming_it = receiver_log.begin();
    for (; incoming_it != receiver_log.end(); ++incoming_it) {
      EXPECT_EQ(expected_it->rtp_timestamp_, incoming_it->rtp_timestamp_);
      EXPECT_EQ(expected_it->event_log_messages_.size(),
                incoming_it->event_log_messages_.size());

      RtcpReceiverEventLogMessages::const_iterator event_incoming_it =
          incoming_it->event_log_messages_.begin();
      RtcpReceiverEventLogMessages::const_iterator event_expected_it =
          expected_it->event_log_messages_.begin();
      for (; event_incoming_it != incoming_it->event_log_messages_.end();
           ++event_incoming_it, ++event_expected_it) {
        EXPECT_EQ(event_expected_it->type, event_incoming_it->type);
        EXPECT_EQ(event_expected_it->event_timestamp,
                  event_incoming_it->event_timestamp);
        if (event_expected_it->type == PACKET_RECEIVED) {
          EXPECT_EQ(event_expected_it->packet_id, event_incoming_it->packet_id);
        } else {
          EXPECT_EQ(event_expected_it->delay_delta,
                    event_incoming_it->delay_delta);
        }
      }
      expected_receiver_log_.pop_front();
      expected_it = expected_receiver_log_.begin();
    }
    called_on_received_receiver_log_ = true;
  }

  virtual void OnReceivedCastFeedback(const RtcpCastMessage& cast_message)
      OVERRIDE {
    EXPECT_EQ(cast_message.media_ssrc, kSenderSsrc);
    EXPECT_EQ(cast_message.ack_frame_id, kAckFrameId);

    MissingFramesAndPacketsMap::const_iterator frame_it =
        cast_message.missing_frames_and_packets.begin();

    EXPECT_TRUE(frame_it != cast_message.missing_frames_and_packets.end());
    EXPECT_EQ(kLostFrameId, frame_it->first);
    EXPECT_EQ(frame_it->second.size(), 1UL);
    EXPECT_EQ(*frame_it->second.begin(), kRtcpCastAllPacketsLost);
    ++frame_it;
    EXPECT_TRUE(frame_it != cast_message.missing_frames_and_packets.end());
    EXPECT_EQ(kFrameIdWithLostPackets, frame_it->first);
    EXPECT_EQ(3UL, frame_it->second.size());
    PacketIdSet::const_iterator packet_it = frame_it->second.begin();
    EXPECT_EQ(kLostPacketId1, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId2, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId3, *packet_it);
    ++frame_it;
    EXPECT_TRUE(frame_it == cast_message.missing_frames_and_packets.end());
    called_on_received_cast_message_ = true;
  }

  bool OnReceivedReceiverLogCalled() const {
    return called_on_received_receiver_log_ && expected_receiver_log_.empty();
  }

  bool OnReceivedCastFeedbackCalled() const {
    return called_on_received_cast_message_;
  }

  void SetExpectedReceiverLog(const RtcpReceiverLogMessage& receiver_log) {
    expected_receiver_log_ = receiver_log;
  }

 private:
  RtcpReceiverLogMessage expected_receiver_log_;
  bool called_on_received_sender_log_;
  bool called_on_received_receiver_log_;
  bool called_on_received_cast_message_;

  DISALLOW_COPY_AND_ASSIGN(RtcpMessageVerification);
};

}  // namespace

class RtcpReceiverTest : public ::testing::Test {
 protected:
  RtcpReceiverTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(
            testing_clock_.get())),
        rtcp_receiver_(new RtcpReceiver(&mock_receiver_feedback_,
                                        kSourceSsrc)) {
    EXPECT_CALL(mock_receiver_feedback_, OnReceivedSenderReport(_)).Times(0);
    EXPECT_CALL(mock_receiver_feedback_, OnReceiverReferenceTimeReport(_))
        .Times(0);
    EXPECT_CALL(mock_receiver_feedback_, OnReceivedCastFeedback(_)).Times(0);
    EXPECT_CALL(mock_receiver_feedback_, OnReceivedDelaySinceLastReport(_, _))
        .Times(0);

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
    expected_report_block_.jitter = kTestJitter;
    expected_report_block_.last_sr = kLastSr;
    expected_report_block_.delay_since_last_sr = kDelayLastSr;
    expected_receiver_reference_report_.remote_ssrc = kSenderSsrc;
    expected_receiver_reference_report_.ntp_seconds = kNtpHigh;
    expected_receiver_reference_report_.ntp_fraction = kNtpLow;
  }

  virtual ~RtcpReceiverTest() {}

  // Injects an RTCP packet into the receiver.
  void InjectRtcpPacket(const uint8* packet, uint16 length) {
    RtcpParser rtcp_parser(packet, length);
    rtcp_receiver_->IncomingRtcpPacket(&rtcp_parser);
  }

  scoped_ptr<base::SimpleTestTickClock> testing_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  MockRtcpReceiverFeedback mock_receiver_feedback_;
  scoped_ptr<RtcpReceiver> rtcp_receiver_;
  RtcpSenderInfo expected_sender_info_;
  RtcpReportBlock expected_report_block_;
  RtcpReceiverReferenceTimeReport expected_receiver_reference_report_;

  DISALLOW_COPY_AND_ASSIGN(RtcpReceiverTest);
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
  InjectRtcpPacket(p.Data(), p.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Expected to be pass through since the sender ssrc match our remote ssrc.
  InjectRtcpPacket(p.Data(), p.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiveReportPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Data(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Data(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectSenderReportWithReportBlockPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddSr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  InjectRtcpPacket(p1.Data(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  InjectRtcpPacket(p1.Data(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_, OnReceivedSenderReport(_)).Times(0);
  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  rtcp_receiver_->SetRemoteSSRC(0);

  TestRtcpPacketBuilder p2;
  p2.AddSr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Receiver report expected to be pass through since the sender ssrc match
  // our local ssrc.
  InjectRtcpPacket(p2.Data(), p2.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Receiver report expected to be pass through since the sender ssrc match
  // our local ssrc.
  InjectRtcpPacket(p2.Data(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectSenderReportPacketWithDlrr) {
  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);
  p.AddXrHeader(kSenderSsrc);
  p.AddXrUnknownBlock();
  p.AddXrExtendedDlrrBlock(kSenderSsrc);
  p.AddXrUnknownBlock();

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p.Data(), p.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  // Enable receiving sender report.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p.Data(), p.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithRrtr) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddXrHeader(kSenderSsrc);
  p1.AddXrRrtrBlock();

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Data(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);
  EXPECT_CALL(mock_receiver_feedback_,
              OnReceiverReferenceTimeReport(
                  expected_receiver_reference_report_)).Times(1);

  // Enable receiving reference time report.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddXrHeader(kSenderSsrc);
  p2.AddXrRrtrBlock();

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Data(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithIntraFrameRequest) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Data(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Data(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithCastFeedback) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddCast(kSenderSsrc, kUnknownSsrc, kTargetDelay);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Data(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);
  EXPECT_CALL(mock_receiver_feedback_, OnReceivedCastFeedback(_)).Times(1);

  // Enable receiving the cast feedback.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddCast(kSenderSsrc, kSourceSsrc, kTargetDelay);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Data(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithCastVerification) {
  RtcpMessageVerification verification;
  RtcpReceiver rtcp_receiver(&verification, kSourceSsrc);

  EXPECT_CALL(verification,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  // Enable receiving the cast feedback.
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddCast(kSenderSsrc, kSourceSsrc, kTargetDelay);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  RtcpParser rtcp_parser(p.Data(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(verification.OnReceivedCastFeedbackCalled());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportWithReceiverLogVerificationBase) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  RtcpMessageVerification verification;
  RtcpReceiver rtcp_receiver(&verification,
                             kSourceSsrc);
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);
  rtcp_receiver.SetCastReceiverEventHistorySize(100);

  RtcpReceiverLogMessage receiver_log;
  RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
  RtcpReceiverEventLogMessage event_log;

  event_log.type = FRAME_ACK_SENT;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
  frame_log.event_log_messages_.push_back(event_log);

  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  event_log.type = PACKET_RECEIVED;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.packet_id = kLostPacketId1;
  frame_log.event_log_messages_.push_back(event_log);

  event_log.type = PACKET_RECEIVED;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.packet_id = kLostPacketId2;
  frame_log.event_log_messages_.push_back(event_log);

  receiver_log.push_back(frame_log);

  verification.SetExpectedReceiverLog(receiver_log);

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddReceiverLog(kSenderSsrc);
  p.AddReceiverFrameLog(kRtpTimestamp, 3, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, FRAME_ACK_SENT, 0);
  p.AddReceiverEventLog(kLostPacketId1, PACKET_RECEIVED, kTimeDelayMs);
  p.AddReceiverEventLog(kLostPacketId2, PACKET_RECEIVED, kTimeDelayMs);

  // Adds duplicated receiver event.
  p.AddReceiverFrameLog(kRtpTimestamp, 3, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, FRAME_ACK_SENT, 0);
  p.AddReceiverEventLog(kLostPacketId1, PACKET_RECEIVED, kTimeDelayMs);
  p.AddReceiverEventLog(kLostPacketId2, PACKET_RECEIVED, kTimeDelayMs);

  EXPECT_CALL(verification,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  RtcpParser rtcp_parser(p.Data(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(verification.OnReceivedReceiverLogCalled());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportWithReceiverLogVerificationMulti) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  RtcpMessageVerification verification;
  RtcpReceiver rtcp_receiver(&verification,
                             kSourceSsrc);
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  RtcpReceiverLogMessage receiver_log;

  for (int j = 0; j < 100; ++j) {
    RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
    RtcpReceiverEventLogMessage event_log;
    event_log.type = FRAME_ACK_SENT;
    event_log.event_timestamp = testing_clock.NowTicks();
    event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
    frame_log.event_log_messages_.push_back(event_log);
    receiver_log.push_back(frame_log);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }

  verification.SetExpectedReceiverLog(receiver_log);

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddReceiverLog(kSenderSsrc);
  for (int i = 0; i < 100; ++i) {
    p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs + i * kTimeDelayMs);
    p.AddReceiverEventLog(kDelayDeltaMs, FRAME_ACK_SENT, 0);
  }

  EXPECT_CALL(verification,
              OnReceivedDelaySinceLastReport(kLastSr, kDelayLastSr)).Times(1);

  RtcpParser rtcp_parser(p.Data(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(verification.OnReceivedReceiverLogCalled());
}

}  // namespace cast
}  // namespace media
