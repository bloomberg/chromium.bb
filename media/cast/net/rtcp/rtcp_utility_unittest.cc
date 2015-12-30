// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/rtcp_utility.h"
#include "media/cast/net/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const uint32_t kSenderSsrc = 0x10203;
static const uint32_t kSourceSsrc = 0x40506;
static const uint32_t kUnknownSsrc = 0xDEAD;
static const base::TimeDelta kTargetDelay =
    base::TimeDelta::FromMilliseconds(100);

class RtcpParserTest : public ::testing::Test {
 protected:
  RtcpParserTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(
            testing_clock_.get())) {
  }

  bool HasAnything(const RtcpParser& parser) {
    return parser.has_sender_report() ||
        parser.has_last_report() ||
        parser.has_receiver_log() ||
        parser.has_cast_message() ||
        parser.has_receiver_reference_time_report();
  }

  void ExpectSenderInfo(const RtcpParser& parser) {
    EXPECT_TRUE(parser.has_sender_report());
    EXPECT_EQ(kNtpHigh, parser.sender_report().ntp_seconds);
    EXPECT_EQ(kNtpLow, parser.sender_report().ntp_fraction);
    EXPECT_EQ(kRtpTimestamp,
              parser.sender_report().rtp_timestamp.lower_32_bits());
    EXPECT_EQ(kSendPacketCount, parser.sender_report().send_packet_count);
    EXPECT_EQ(kSendOctetCount, parser.sender_report().send_octet_count);
  }

  void ExpectLastReport(const RtcpParser& parser) {
    EXPECT_TRUE(parser.has_last_report());
    EXPECT_EQ(kLastSr, parser.last_report());
    EXPECT_EQ(kDelayLastSr, parser.delay_since_last_report());
  }

  void ExpectReceiverReference(const RtcpParser& parser) {
    EXPECT_TRUE(parser.has_receiver_reference_time_report());
    EXPECT_EQ(kSenderSsrc, parser.receiver_reference_time_report().remote_ssrc);
    EXPECT_EQ(kNtpHigh, parser.receiver_reference_time_report().ntp_seconds);
    EXPECT_EQ(kNtpLow, parser.receiver_reference_time_report().ntp_fraction);
  }

  void ExpectCastFeedback(const RtcpParser& parser) {
    EXPECT_TRUE(parser.has_cast_message());
    EXPECT_EQ(kSenderSsrc, parser.cast_message().media_ssrc);
    EXPECT_EQ(kAckFrameId, parser.cast_message().ack_frame_id);

    MissingFramesAndPacketsMap::const_iterator frame_it =
        parser.cast_message().missing_frames_and_packets.begin();

    EXPECT_TRUE(
        frame_it != parser.cast_message().missing_frames_and_packets.end());
    EXPECT_EQ(kLostFrameId, frame_it->first);
    EXPECT_EQ(frame_it->second.size(), 1UL);
    EXPECT_EQ(*frame_it->second.begin(), kRtcpCastAllPacketsLost);
    ++frame_it;
    EXPECT_TRUE(
        frame_it != parser.cast_message().missing_frames_and_packets.end());
    EXPECT_EQ(kFrameIdWithLostPackets, frame_it->first);
    EXPECT_EQ(3UL, frame_it->second.size());
    PacketIdSet::const_iterator packet_it = frame_it->second.begin();
    EXPECT_EQ(kLostPacketId1, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId2, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId3, *packet_it);
    ++frame_it;
    EXPECT_TRUE(
        frame_it == parser.cast_message().missing_frames_and_packets.end());
  }

  void ExpectReceiverLog(const RtcpParser& parser,
                         const RtcpReceiverLogMessage& expected_receiver_log) {
    EXPECT_TRUE(parser.has_receiver_log());
    EXPECT_EQ(expected_receiver_log.size(), parser.receiver_log().size());
    RtcpReceiverLogMessage::const_iterator expected_it =
        expected_receiver_log.begin();
    RtcpReceiverLogMessage::const_iterator incoming_it =
        parser.receiver_log().begin();
    for (; incoming_it != parser.receiver_log().end();
         ++incoming_it, ++expected_it) {
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
    }
  }

  scoped_ptr<base::SimpleTestTickClock> testing_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RtcpParserTest);
};

TEST_F(RtcpParserTest, BrokenPacketIsIgnored) {
  const char bad_packet[] = {0, 0, 0, 0};
  RtcpParser parser(kSourceSsrc, kSenderSsrc);
  base::BigEndianReader reader(bad_packet, sizeof(bad_packet));
  EXPECT_FALSE(parser.Parse(&reader));
}

TEST_F(RtcpParserTest, UnknownBlockIgnored) {
  // Only unknown data, nothing happens.
  TestRtcpPacketBuilder p;
  p.AddUnknownBlock();
  RtcpParser parser1(kSourceSsrc, 0);
  EXPECT_TRUE(parser1.Parse(p.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  // Add valid sender report *after* unknown data - should work fine.
  p.AddSr(kSenderSsrc, 0);
  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p.Reader()));
  ExpectSenderInfo(parser2);
}

TEST_F(RtcpParserTest, InjectSenderReportPacket) {
  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);

  // Expected to be ignored since the sender ssrc does not match our
  // remote ssrc.
  RtcpParser parser1(kSourceSsrc, 0);
  EXPECT_TRUE(parser1.Parse(p.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  // Expected to be pass through since the sender ssrc match our remote ssrc.
  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p.Reader()));
  ExpectSenderInfo(parser2);
}

TEST_F(RtcpParserTest, InjectReceiveReportPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  RtcpParser parser1(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser1.Parse(p1.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p2.Reader()));
  ExpectLastReport(parser2);
}

TEST_F(RtcpParserTest, InjectSenderReportWithReportBlockPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddSr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  RtcpParser parser1(kSourceSsrc, 0);
  EXPECT_TRUE(parser1.Parse(p1.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p1.Reader()));
  ExpectSenderInfo(parser2);
  EXPECT_FALSE(parser2.has_last_report());

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Report block expected to be ignored too since it's a part of the
  // sender report.
  TestRtcpPacketBuilder p2;
  p2.AddSr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  RtcpParser parser3(kSourceSsrc, 0);
  EXPECT_TRUE(parser3.Parse(p2.Reader()));
  EXPECT_FALSE(parser3.has_last_report());

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Report block expected to be pass through since the sender ssrc match
  // our local ssrc.
  RtcpParser parser4(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser4.Parse(p2.Reader()));
  ExpectSenderInfo(parser4);
  ExpectLastReport(parser4);
}

TEST_F(RtcpParserTest, InjectSenderReportPacketWithDlrr) {
  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);
  p.AddXrHeader(kSenderSsrc);
  p.AddXrUnknownBlock();
  p.AddXrExtendedDlrrBlock(kSenderSsrc);
  p.AddXrUnknownBlock();

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  RtcpParser parser1(kSourceSsrc, 0);
  EXPECT_TRUE(parser1.Parse(p.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  // Expected to be pass through since the sender ssrc match our local ssrc.
  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p.Reader()));
  ExpectSenderInfo(parser2);
  // DLRRs are ignored.
  EXPECT_FALSE(parser2.has_last_report());
}

TEST_F(RtcpParserTest, InjectReceiverReportPacketWithRrtr) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddXrHeader(kSenderSsrc);
  p1.AddXrRrtrBlock();

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  RtcpParser parser1(kSourceSsrc, 0);
  EXPECT_TRUE(parser1.Parse(p1.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddXrHeader(kSenderSsrc);
  p2.AddXrRrtrBlock();

  // Expected to be pass through since the sender ssrc match our local ssrc.
  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p2.Reader()));
  ExpectLastReport(parser2);
  ExpectReceiverReference(parser2);
}

TEST_F(RtcpParserTest, InjectReceiverReportPacketWithIntraFrameRequest) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  RtcpParser parser1(kSourceSsrc, 0);
  EXPECT_TRUE(parser1.Parse(p1.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p2.Reader()));
  ExpectLastReport(parser2);
}

TEST_F(RtcpParserTest, InjectReceiverReportPacketWithCastFeedback) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddCast(kSenderSsrc, kUnknownSsrc, kTargetDelay);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  RtcpParser parser1(kSourceSsrc, 0);
  EXPECT_TRUE(parser1.Parse(p1.Reader()));
  EXPECT_FALSE(HasAnything(parser1));

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddCast(kSenderSsrc, kSourceSsrc, kTargetDelay);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  RtcpParser parser2(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser2.Parse(p2.Reader()));
  ExpectLastReport(parser2);
  ExpectCastFeedback(parser2);
}

TEST_F(RtcpParserTest, InjectReceiverReportWithReceiverLogVerificationBase) {
  static const uint32_t kTimeBaseMs = 12345678;
  static const uint32_t kTimeDelayMs = 10;
  static const uint32_t kDelayDeltaMs = 123;
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  RtcpReceiverLogMessage receiver_log;
  RtcpReceiverFrameLogMessage frame_log(RtpTimeTicks().Expand(kRtpTimestamp));
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

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddReceiverLog(kSenderSsrc);
  p.AddReceiverFrameLog(kRtpTimestamp, 3, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, FRAME_ACK_SENT, 0);
  p.AddReceiverEventLog(kLostPacketId1, PACKET_RECEIVED, kTimeDelayMs);
  p.AddReceiverEventLog(kLostPacketId2, PACKET_RECEIVED, kTimeDelayMs);

  RtcpParser parser(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser.Parse(p.Reader()));
  ExpectReceiverLog(parser, receiver_log);
}

TEST_F(RtcpParserTest, InjectReceiverReportWithReceiverLogVerificationMulti) {
  static const uint32_t kTimeBaseMs = 12345678;
  static const uint32_t kTimeDelayMs = 10;
  static const int kDelayDeltaMs = 123;  // To be varied for every frame.
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  RtcpReceiverLogMessage receiver_log;

  for (int j = 0; j < 100; ++j) {
    RtcpReceiverFrameLogMessage frame_log(RtpTimeTicks().Expand(kRtpTimestamp));
    RtcpReceiverEventLogMessage event_log;
    event_log.type = FRAME_ACK_SENT;
    event_log.event_timestamp = testing_clock.NowTicks();
    event_log.delay_delta =
        base::TimeDelta::FromMilliseconds((j - 50) * kDelayDeltaMs);
    frame_log.event_log_messages_.push_back(event_log);
    receiver_log.push_back(frame_log);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddReceiverLog(kSenderSsrc);
  for (int i = 0; i < 100; ++i) {
    p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs + i * kTimeDelayMs);
    const int delay = (i - 50) * kDelayDeltaMs;
    p.AddReceiverEventLog(static_cast<uint16_t>(delay), FRAME_ACK_SENT, 0);
  }

  RtcpParser parser(kSourceSsrc, kSenderSsrc);
  EXPECT_TRUE(parser.Parse(p.Reader()));
  ExpectReceiverLog(parser, receiver_log);
}

TEST(RtcpUtilityTest, NtpAndTime) {
  const int64_t kSecondsbetweenYear1900and2010 = INT64_C(40176 * 24 * 60 * 60);
  const int64_t kSecondsbetweenYear1900and2030 = INT64_C(47481 * 24 * 60 * 60);

  uint32_t ntp_seconds_1 = 0;
  uint32_t ntp_fraction_1 = 0;
  base::TimeTicks input_time = base::TimeTicks::Now();
  ConvertTimeTicksToNtp(input_time, &ntp_seconds_1, &ntp_fraction_1);

  // Verify absolute value.
  EXPECT_GT(ntp_seconds_1, kSecondsbetweenYear1900and2010);
  EXPECT_LT(ntp_seconds_1, kSecondsbetweenYear1900and2030);

  base::TimeTicks out_1 = ConvertNtpToTimeTicks(ntp_seconds_1, ntp_fraction_1);
  EXPECT_EQ(input_time, out_1);  // Verify inverse.

  base::TimeDelta time_delta = base::TimeDelta::FromMilliseconds(1000);
  input_time += time_delta;

  uint32_t ntp_seconds_2 = 0;
  uint32_t ntp_fraction_2 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_2, &ntp_fraction_2);
  base::TimeTicks out_2 = ConvertNtpToTimeTicks(ntp_seconds_2, ntp_fraction_2);
  EXPECT_EQ(input_time, out_2);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_2 - out_1), time_delta);
  EXPECT_EQ((ntp_seconds_2 - ntp_seconds_1), UINT32_C(1));
  EXPECT_NEAR(ntp_fraction_2, ntp_fraction_1, 1);

  time_delta = base::TimeDelta::FromMilliseconds(500);
  input_time += time_delta;

  uint32_t ntp_seconds_3 = 0;
  uint32_t ntp_fraction_3 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_3, &ntp_fraction_3);
  base::TimeTicks out_3 = ConvertNtpToTimeTicks(ntp_seconds_3, ntp_fraction_3);
  EXPECT_EQ(input_time, out_3);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_3 - out_2), time_delta);
  EXPECT_NEAR((ntp_fraction_3 - ntp_fraction_2), 0xffffffff / 2, 1);
}

}  // namespace cast
}  // namespace media
