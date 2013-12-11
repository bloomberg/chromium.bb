// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp_sender/rtp_packetizer/rtp_packetizer.h"

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_config.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtp_sender/packet_storage/packet_storage.h"
#include "media/cast/net/rtp_sender/rtp_packetizer/test/rtp_header_parser.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

static const int kPayload = 127;
static const uint32 kTimestampMs = 10;
static const uint16 kSeqNum = 33;
static const int kMaxPacketLength = 1500;
static const int kSsrc = 0x12345;
static const unsigned int kFrameSize = 5000;
static const int kMaxPacketStorageTimeMs = 300;

class TestRtpPacketTransport : public PacedPacketSender {
 public:
  explicit TestRtpPacketTransport(RtpPacketizerConfig config)
       : config_(config),
         sequence_number_(kSeqNum),
         packets_sent_(0),
         expected_number_of_packets_(0),
         expected_packet_id_(0),
         expected_frame_id_(0) {}

  void VerifyRtpHeader(const RtpCastTestHeader& rtp_header) {
    VerifyCommonRtpHeader(rtp_header);
    VerifyCastRtpHeader(rtp_header);
  }

  void VerifyCommonRtpHeader(const RtpCastTestHeader& rtp_header) {
    EXPECT_EQ(expected_number_of_packets_ == packets_sent_,
        rtp_header.marker);
    EXPECT_EQ(kPayload, rtp_header.payload_type);
    EXPECT_EQ(sequence_number_, rtp_header.sequence_number);
    EXPECT_EQ(kTimestampMs * 90, rtp_header.rtp_timestamp);
    EXPECT_EQ(config_.ssrc, rtp_header.ssrc);
    EXPECT_EQ(0, rtp_header.num_csrcs);
  }

  void VerifyCastRtpHeader(const RtpCastTestHeader& rtp_header) {
    EXPECT_FALSE(rtp_header.is_key_frame);
    EXPECT_EQ(expected_frame_id_, rtp_header.frame_id);
    EXPECT_EQ(expected_packet_id_, rtp_header.packet_id);
    EXPECT_EQ(expected_number_of_packets_ - 1, rtp_header.max_packet_id);
    EXPECT_TRUE(rtp_header.is_reference);
    EXPECT_EQ(expected_frame_id_ - 1u, rtp_header.reference_frame_id);
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE {
    EXPECT_EQ(expected_number_of_packets_, static_cast<int>(packets.size()));
    PacketList::const_iterator it = packets.begin();
    for (; it != packets.end(); ++it) {
      ++packets_sent_;
      RtpHeaderParser parser(it->data(), it->size());
      RtpCastTestHeader rtp_header;
      parser.Parse(&rtp_header);
      VerifyRtpHeader(rtp_header);
      ++sequence_number_;
      ++expected_packet_id_;
    }
    return true;
  }

  virtual bool ResendPackets(const PacketList& packets) OVERRIDE {
    EXPECT_TRUE(false);
    return false;
  }

  virtual bool SendRtcpPacket(const std::vector<uint8>& packet) OVERRIDE {
    EXPECT_TRUE(false);
    return false;
  }

  void SetExpectedNumberOfPackets(int num) {
    expected_number_of_packets_ = num;
  }

  RtpPacketizerConfig config_;
  uint32 sequence_number_;
  int packets_sent_;
  int expected_number_of_packets_;
  // Assuming packets arrive in sequence.
  int expected_packet_id_;
  uint32 expected_frame_id_;
};

class RtpPacketizerTest : public ::testing::Test {
 protected:
  RtpPacketizerTest()
      :video_frame_(),
       packet_storage_(&testing_clock_, kMaxPacketStorageTimeMs) {
    config_.sequence_number = kSeqNum;
    config_.ssrc = kSsrc;
    config_.payload_type = kPayload;
    config_.max_payload_length = kMaxPacketLength;
    transport_.reset(new TestRtpPacketTransport(config_));
    rtp_packetizer_.reset(
        new RtpPacketizer(transport_.get(), &packet_storage_, config_));
  }

  virtual ~RtpPacketizerTest() {}

  virtual void SetUp() {
    video_frame_.key_frame = false;
    video_frame_.frame_id = 0;
    video_frame_.last_referenced_frame_id = kStartFrameId;
    video_frame_.data.assign(kFrameSize, 123);
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_ptr<RtpPacketizer> rtp_packetizer_;
  RtpPacketizerConfig config_;
  scoped_ptr<TestRtpPacketTransport> transport_;
  EncodedVideoFrame video_frame_;
  PacketStorage packet_storage_;
};

TEST_F(RtpPacketizerTest, SendStandardPackets) {
  int expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->SetExpectedNumberOfPackets(expected_num_of_packets);

  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(kTimestampMs);
  rtp_packetizer_->IncomingEncodedVideoFrame(&video_frame_, time);
}

TEST_F(RtpPacketizerTest, Stats) {
  EXPECT_FALSE(rtp_packetizer_->send_packets_count());
  EXPECT_FALSE(rtp_packetizer_->send_octet_count());
  // Insert packets at varying lengths.
  int expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->SetExpectedNumberOfPackets(expected_num_of_packets);

  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(kTimestampMs));
  rtp_packetizer_->IncomingEncodedVideoFrame(&video_frame_,
                                             testing_clock_.NowTicks());
  EXPECT_EQ(expected_num_of_packets, rtp_packetizer_->send_packets_count());
  EXPECT_EQ(kFrameSize, rtp_packetizer_->send_octet_count());
}

}  // namespace cast
}  // namespace media
