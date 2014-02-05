// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/memory/scoped_ptr.h"
#include "media/cast/rtp_receiver/rtp_parser/rtp_parser.h"
#include "media/cast/rtp_receiver/rtp_parser/test/rtp_packet_builder.h"
#include "media/cast/rtp_receiver/rtp_receiver.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

static const size_t kPacketLength = 1500;
static const int kTestPayloadType = 127;
static const uint32 kTestSsrc = 1234;
static const uint32 kTestTimestamp = 111111;
static const uint16 kTestSeqNum = 4321;
static const uint8 kRefFrameId = 17;

class RtpDataTest : public RtpData {
 public:
  RtpDataTest() { expected_header_.reset(new RtpCastHeader()); }

  virtual ~RtpDataTest() {}

  void SetExpectedHeader(const RtpCastHeader& cast_header) {
    memcpy(expected_header_.get(), &cast_header, sizeof(RtpCastHeader));
  }

  virtual void OnReceivedPayloadData(const uint8* payloadData,
                                     size_t payloadSize,
                                     const RtpCastHeader* rtpHeader) OVERRIDE {
    VerifyCommonHeader(*rtpHeader);
    VerifyCastHeader(*rtpHeader);
  }

  void VerifyCommonHeader(const RtpCastHeader& parsed_header) {
    EXPECT_EQ(expected_header_->packet_id == expected_header_->max_packet_id,
              parsed_header.webrtc.header.markerBit);
    EXPECT_EQ(kTestPayloadType, parsed_header.webrtc.header.payloadType);
    EXPECT_EQ(kTestSsrc, parsed_header.webrtc.header.ssrc);
    EXPECT_EQ(0, parsed_header.webrtc.header.numCSRCs);
  }

  void VerifyCastHeader(const RtpCastHeader& parsed_header) {
    EXPECT_EQ(expected_header_->is_key_frame, parsed_header.is_key_frame);
    EXPECT_EQ(expected_header_->frame_id, parsed_header.frame_id);
    EXPECT_EQ(expected_header_->packet_id, parsed_header.packet_id);
    EXPECT_EQ(expected_header_->max_packet_id, parsed_header.max_packet_id);
    EXPECT_EQ(expected_header_->is_reference, parsed_header.is_reference);
  }

 private:
  scoped_ptr<RtpCastHeader> expected_header_;

  DISALLOW_COPY_AND_ASSIGN(RtpDataTest);
};

class RtpParserTest : public ::testing::Test {
 protected:
  RtpParserTest() {
    PopulateConfig();
    rtp_data_.reset(new RtpDataTest());
    rtp_parser_.reset(new RtpParser(rtp_data_.get(), config_));
    cast_header_.is_reference = true;
    cast_header_.reference_frame_id = kRefFrameId;
    packet_builder_.SetSsrc(kTestSsrc);
    packet_builder_.SetReferenceFrameId(kRefFrameId, true);
    packet_builder_.SetSequenceNumber(kTestSeqNum);
    packet_builder_.SetTimestamp(kTestTimestamp);
    packet_builder_.SetPayloadType(kTestPayloadType);
    packet_builder_.SetMarkerBit(true);  // Only one packet.
  }

  virtual ~RtpParserTest() {}

  void PopulateConfig() {
    config_.payload_type = kTestPayloadType;
    config_.ssrc = kTestSsrc;
  }

  scoped_ptr<RtpDataTest> rtp_data_;
  RtpPacketBuilder packet_builder_;
  scoped_ptr<RtpParser> rtp_parser_;
  RtpParserConfig config_;
  RtpCastHeader cast_header_;
};

TEST_F(RtpParserTest, ParseDefaultCastPacket) {
  // Build generic data packet.
  uint8 packet[kPacketLength];
  packet_builder_.BuildHeader(packet, kPacketLength);
  // Parse packet as is.
  RtpCastHeader rtp_header;
  rtp_data_->SetExpectedHeader(cast_header_);
  EXPECT_TRUE(rtp_parser_->ParsePacket(packet, kPacketLength, &rtp_header));
}

TEST_F(RtpParserTest, ParseNonDefaultCastPacket) {
  // Build generic data packet.
  uint8 packet[kPacketLength];
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameId(10);
  packet_builder_.SetPacketId(5);
  packet_builder_.SetMaxPacketId(15);
  packet_builder_.SetMarkerBit(false);
  packet_builder_.BuildHeader(packet, kPacketLength);
  cast_header_.is_key_frame = true;
  cast_header_.frame_id = 10;
  cast_header_.packet_id = 5;
  cast_header_.max_packet_id = 15;
  rtp_data_->SetExpectedHeader(cast_header_);
  // Parse packet as is.
  RtpCastHeader rtp_header;
  EXPECT_TRUE(rtp_parser_->ParsePacket(packet, kPacketLength, &rtp_header));
}

TEST_F(RtpParserTest, TooBigPacketId) {
  // Build generic data packet.
  uint8 packet[kPacketLength];
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameId(10);
  packet_builder_.SetPacketId(15);
  packet_builder_.SetMaxPacketId(5);
  packet_builder_.BuildHeader(packet, kPacketLength);
  // Parse packet as is.
  RtpCastHeader rtp_header;
  EXPECT_FALSE(rtp_parser_->ParsePacket(packet, kPacketLength, &rtp_header));
}

TEST_F(RtpParserTest, MaxPacketId) {
  // Build generic data packet.
  uint8 packet[kPacketLength];
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameId(10);
  packet_builder_.SetPacketId(65535);
  packet_builder_.SetMaxPacketId(65535);
  packet_builder_.BuildHeader(packet, kPacketLength);
  cast_header_.is_key_frame = true;
  cast_header_.frame_id = 10;
  cast_header_.packet_id = 65535;
  cast_header_.max_packet_id = 65535;
  rtp_data_->SetExpectedHeader(cast_header_);
  // Parse packet as is.
  RtpCastHeader rtp_header;
  EXPECT_TRUE(rtp_parser_->ParsePacket(packet, kPacketLength, &rtp_header));
}

TEST_F(RtpParserTest, InvalidPayloadType) {
  // Build generic data packet.
  uint8 packet[kPacketLength];
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameId(10);
  packet_builder_.SetPacketId(65535);
  packet_builder_.SetMaxPacketId(65535);
  packet_builder_.SetPayloadType(kTestPayloadType - 1);
  packet_builder_.BuildHeader(packet, kPacketLength);
  // Parse packet as is.
  RtpCastHeader rtp_header;
  EXPECT_FALSE(rtp_parser_->ParsePacket(packet, kPacketLength, &rtp_header));
}

TEST_F(RtpParserTest, InvalidSsrc) {
  // Build generic data packet.
  uint8 packet[kPacketLength];
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameId(10);
  packet_builder_.SetPacketId(65535);
  packet_builder_.SetMaxPacketId(65535);
  packet_builder_.SetSsrc(kTestSsrc - 1);
  packet_builder_.BuildHeader(packet, kPacketLength);
  // Parse packet as is.
  RtpCastHeader rtp_header;
  EXPECT_FALSE(rtp_parser_->ParsePacket(packet, kPacketLength, &rtp_header));
}

TEST_F(RtpParserTest, ParseCastPacketWithoutReference) {
  cast_header_.is_reference = false;
  cast_header_.reference_frame_id = 0;
  packet_builder_.SetReferenceFrameId(kRefFrameId, false);

  // Build generic data packet.
  uint8 packet[kPacketLength];
  packet_builder_.BuildHeader(packet, kPacketLength);
  // Parse packet as is.
  RtpCastHeader rtp_header;
  rtp_data_->SetExpectedHeader(cast_header_);
  EXPECT_TRUE(rtp_parser_->ParsePacket(packet, kPacketLength, &rtp_header));
}

}  //  namespace cast
}  //  namespace media
