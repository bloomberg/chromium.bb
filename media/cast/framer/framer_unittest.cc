// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/framer/framer.h"
#include "media/cast/rtp_receiver/mock_rtp_payload_feedback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class FramerTest : public ::testing::Test {
 protected:
  FramerTest()
      : mock_rtp_payload_feedback_(),
        framer_(&testing_clock_, &mock_rtp_payload_feedback_, 0, true, 0) {
    // Build a default one packet frame - populate webrtc header.
    rtp_header_.is_key_frame = false;
    rtp_header_.frame_id = 0;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
    payload_.assign(kMaxIpPacketSize, 0);

    EXPECT_CALL(mock_rtp_payload_feedback_, CastFeedback(testing::_))
        .WillRepeatedly(testing::Return());
  }

  virtual ~FramerTest() {}

  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  MockRtpPayloadFeedback mock_rtp_payload_feedback_;
  Framer framer_;
  base::SimpleTestTickClock testing_clock_;

  DISALLOW_COPY_AND_ASSIGN(FramerTest);
};

TEST_F(FramerTest, EmptyState) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
}

TEST_F(FramerTest, AlwaysStartWithKey) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool duplicate = false;

  // Insert non key first frame.
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  rtp_header_.frame_id = 1;
  rtp_header_.is_key_frame = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(1u, frame.frame_id);
  EXPECT_TRUE(frame.key_frame);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, CompleteFrame) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool duplicate = false;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(0u, frame.frame_id);
  EXPECT_TRUE(frame.key_frame);
  framer_.ReleaseFrame(frame.frame_id);

  // Incomplete delta.
  ++rtp_header_.frame_id;
  rtp_header_.is_key_frame = false;
  rtp_header_.max_packet_id = 2;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));

  // Complete delta - can't skip, as incomplete sequence.
  ++rtp_header_.frame_id;
  rtp_header_.max_packet_id = 0;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
}

TEST_F(FramerTest, DuplicatePackets) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool duplicate = false;

  // Start with an incomplete key frame.
  rtp_header_.is_key_frame = true;
  rtp_header_.max_packet_id = 1;
  duplicate = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));

  // Add same packet again in incomplete key frame.
  duplicate = false;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));

  // Complete key frame.
  rtp_header_.packet_id = 1;
  duplicate = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_EQ(0u, frame.frame_id);

  // Add same packet again in complete key frame.
  duplicate = false;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_EQ(0u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Incomplete delta frame.
  ++rtp_header_.frame_id;
  rtp_header_.packet_id = 0;
  rtp_header_.is_key_frame = false;
  duplicate = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));

  // Add same packet again in incomplete delta frame.
  duplicate = false;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));

  // Complete delta frame.
  rtp_header_.packet_id = 1;
  duplicate = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_EQ(1u, frame.frame_id);

  // Add same packet again in complete delta frame.
  duplicate = false;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_EQ(1u, frame.frame_id);
}

TEST_F(FramerTest, ContinuousSequence) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool duplicate = false;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(0u, frame.frame_id);
  EXPECT_TRUE(frame.key_frame);
  framer_.ReleaseFrame(frame.frame_id);

  // Complete - not continuous.
  rtp_header_.frame_id = 2;
  rtp_header_.is_key_frame = false;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
}

TEST_F(FramerTest, Wrap) {
  // Insert key frame, frame_id = 255 (will jump to that)
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool duplicate = false;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 255u;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(255u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Insert wrapped delta frame - should be continuous.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = 256;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(256u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, Reset) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool duplicate = false;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  complete = framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(complete);
  framer_.Reset();
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
}

TEST_F(FramerTest, RequireKeyAfterReset) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool duplicate = false;

  framer_.Reset();

  // Start with a complete key frame.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = 0u;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  rtp_header_.frame_id = 1;
  rtp_header_.is_key_frame = true;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
}

TEST_F(FramerTest, BasicNonLastReferenceId) {
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool duplicate = false;

  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);

  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  framer_.ReleaseFrame(frame.frame_id);

  rtp_header_.is_key_frame = false;
  rtp_header_.is_reference = true;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.frame_id = 5u;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);

  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_FALSE(next_frame);
}

TEST_F(FramerTest, InOrderReferenceFrameSelection) {
  // Create pattern: 0, 1, 4, 5.
  transport::EncodedVideoFrame frame;
  bool next_frame = false;
  bool duplicate = false;

  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = 1;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);

  // Insert frame #2 partially.
  rtp_header_.frame_id = 2;
  rtp_header_.max_packet_id = 1;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  rtp_header_.frame_id = 4;
  rtp_header_.max_packet_id = 0;
  rtp_header_.is_reference = true;
  rtp_header_.reference_frame_id = 0;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_EQ(0u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(1u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_FALSE(next_frame);
  EXPECT_EQ(4u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  // Insert remaining packet of frame #2 - should no be continuous.
  rtp_header_.frame_id = 2;
  rtp_header_.packet_id = 1;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  rtp_header_.is_reference = false;
  rtp_header_.frame_id = 5;
  rtp_header_.packet_id = 0;
  rtp_header_.max_packet_id = 0;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(5u, frame.frame_id);
}

TEST_F(FramerTest, AudioWrap) {
  // All audio frames are marked as key frames.
  transport::EncodedAudioFrame frame;
  bool next_frame = false;
  bool duplicate = false;

  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 254;

  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(254u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  rtp_header_.frame_id = 255;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);

  // Insert wrapped frame - should be continuous.
  rtp_header_.frame_id = 256;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);

  EXPECT_TRUE(framer_.GetEncodedAudioFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(255u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  EXPECT_TRUE(framer_.GetEncodedAudioFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(256u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, AudioWrapWithMissingFrame) {
  // All audio frames are marked as key frames.
  transport::EncodedAudioFrame frame;
  bool next_frame = false;
  bool duplicate = false;

  // Insert and get first packet.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 253;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(253u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Insert third and fourth packets.
  rtp_header_.frame_id = 255;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);
  rtp_header_.frame_id = 256;
  framer_.InsertPacket(
      payload_.data(), payload_.size(), rtp_header_, &duplicate);

  // Get third and fourth packets.
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(&frame, &next_frame));
  EXPECT_FALSE(next_frame);
  EXPECT_EQ(255u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(&frame, &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(256u, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

}  // namespace cast
}  // namespace media
