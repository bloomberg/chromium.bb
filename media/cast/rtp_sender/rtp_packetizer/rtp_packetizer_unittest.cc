// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_sender/rtp_packetizer/rtp_packetizer.h"

#include <gtest/gtest.h>

#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/rtp_common/rtp_defines.h"
#include "media/cast/rtp_sender/packet_storage/packet_storage.h"
#include "media/cast/rtp_sender/rtp_packetizer/test/rtp_header_parser.h"

namespace media {
namespace cast {

static const int kPayload = 127;
static const uint32 kTimestamp = 10;
static const uint16 kSeqNum = 33;
static const int kTimeOffset = 22222;
static const int kMaxPacketLength = 1500;
static const bool kMarkerBit = true;
static const int kSsrc = 0x12345;
static const uint8 kFrameId = 1;
static const unsigned int kFrameSize = 5000;
static const int kTotalHeaderLength = 19;
static const int kMaxPacketStorageTimeMs = 300;

class TestRtpPacketTransport : public PacedPacketSender {
 public:
  explicit TestRtpPacketTransport(RtpPacketizerConfig config)
       : config_(config),
         sequence_number_(kSeqNum),
         packets_sent_(0),
         expected_number_of_packets_(0) {}

  void VerifyRtpHeader(const RtpCastHeader& rtp_header) {
    VerifyCommonRtpHeader(rtp_header);
    VerifyCastRtpHeader(rtp_header);
  }

  void VerifyCommonRtpHeader(const RtpCastHeader& rtp_header) {
    EXPECT_EQ(expected_number_of_packets_ == packets_sent_,
        rtp_header.webrtc.header.markerBit);
    EXPECT_EQ(kPayload, rtp_header.webrtc.header.payloadType);
    EXPECT_EQ(sequence_number_, rtp_header.webrtc.header.sequenceNumber);
    EXPECT_EQ(kTimestamp * 90, rtp_header.webrtc.header.timestamp);
    EXPECT_EQ(config_.ssrc, rtp_header.webrtc.header.ssrc);
    EXPECT_EQ(0, rtp_header.webrtc.header.numCSRCs);
  }

  void VerifyCastRtpHeader(const RtpCastHeader& rtp_header) {
    // TODO(mikhal)
  }

  virtual bool SendPacket(const std::vector<uint8>& packet,
                          int num_packets) OVERRIDE {
    EXPECT_EQ(expected_number_of_packets_, num_packets);
    ++packets_sent_;
    RtpHeaderParser parser(packet.data(), packet.size());
    RtpCastHeader rtp_header;
    parser.Parse(&rtp_header);
    VerifyRtpHeader(rtp_header);
    ++sequence_number_;
    return true;
  }

  virtual bool ResendPacket(const std::vector<uint8>& packet,
                            int num_of_packets) OVERRIDE {
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
};

class RtpPacketizerTest : public ::testing::Test {
 protected:
  RtpPacketizerTest()
      :video_frame_(),
       packet_storage_(kMaxPacketStorageTimeMs) {
    config_.sequence_number = kSeqNum;
    config_.ssrc = kSsrc;
    config_.payload_type = kPayload;
    config_.max_payload_length = kMaxPacketLength;
    transport_.reset(new TestRtpPacketTransport(config_));
    rtp_packetizer_.reset(
        new RtpPacketizer(transport_.get(), &packet_storage_, config_));
  }

  ~RtpPacketizerTest() {}

  void SetUp() {
    video_frame_.key_frame = false;
    video_frame_.frame_id = kFrameId;
    video_frame_.last_referenced_frame_id = kFrameId - 1;
    video_frame_.data.assign(kFrameSize, 123);
  }

  scoped_ptr<RtpPacketizer> rtp_packetizer_;
  RtpPacketizerConfig config_;
  scoped_ptr<TestRtpPacketTransport> transport_;
  EncodedVideoFrame video_frame_;
  PacketStorage packet_storage_;
};

TEST_F(RtpPacketizerTest, SendStandardPackets) {
  int expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->SetExpectedNumberOfPackets(expected_num_of_packets);
  rtp_packetizer_->IncomingEncodedVideoFrame(video_frame_, kTimestamp);
}

TEST_F(RtpPacketizerTest, Stats) {
  EXPECT_FALSE(rtp_packetizer_->send_packets_count());
  EXPECT_FALSE(rtp_packetizer_->send_octet_count());
  // Insert packets at varying lengths.
  unsigned int expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->SetExpectedNumberOfPackets(expected_num_of_packets);
  rtp_packetizer_->IncomingEncodedVideoFrame(video_frame_, kTimestamp);
  EXPECT_EQ(expected_num_of_packets, rtp_packetizer_->send_packets_count());
  EXPECT_EQ(kFrameSize, rtp_packetizer_->send_octet_count());
}

}  // namespace cast
}  // namespace media
