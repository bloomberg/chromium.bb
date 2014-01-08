// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/frame_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class FrameBufferTest : public ::testing::Test {
 protected:
  FrameBufferTest() {}

  virtual ~FrameBufferTest() {}

  virtual void SetUp() {
    payload_.assign(kIpPacketSize, 0);

    // Build a default one packet frame - populate webrtc header.
    rtp_header_.is_key_frame = false;
    rtp_header_.frame_id = 0;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
  }

  FrameBuffer buffer_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
};

TEST_F(FrameBufferTest, EmptyBuffer) {
  EXPECT_FALSE(buffer_.Complete());
  EXPECT_FALSE(buffer_.is_key_frame());
  EncodedVideoFrame frame;
  uint32 rtp_timestamp;
  EXPECT_FALSE(buffer_.GetEncodedVideoFrame(&frame, &rtp_timestamp));
}

TEST_F(FrameBufferTest, DefaultOnePacketFrame) {
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(buffer_.Complete());
  EXPECT_FALSE(buffer_.is_key_frame());
  EncodedVideoFrame frame;
  uint32 rtp_timestamp;
  EXPECT_TRUE(buffer_.GetEncodedVideoFrame(&frame, &rtp_timestamp));
  EXPECT_EQ(payload_.size(), frame.data.size());
}

TEST_F(FrameBufferTest, MultiplePacketFrame) {
  rtp_header_.is_key_frame = true;
  rtp_header_.max_packet_id = 2;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  EXPECT_TRUE(buffer_.Complete());
  EXPECT_TRUE(buffer_.is_key_frame());
  EncodedVideoFrame frame;
  uint32 rtp_timestamp;
  EXPECT_TRUE(buffer_.GetEncodedVideoFrame(&frame, &rtp_timestamp));
  EXPECT_EQ(3 * payload_.size(), frame.data.size());
}

TEST_F(FrameBufferTest, InCompleteFrame) {
  rtp_header_.max_packet_id = 4;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  // Increment again - skip packet #2.
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_FALSE(buffer_.Complete());
  // Insert missing packet.
  rtp_header_.packet_id = 2;
  buffer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(buffer_.Complete());
}

}  // namespace media
}  // namespace cast
