// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Joint encoder and decoder testing.
// These tests operate directly on the VP8 encoder and decoder, not the
// transport layer, and are targeted at validating the bit stream.

#include <gtest/gtest.h>

#include "base/memory/scoped_ptr.h"
#include "media/cast/test/video_utility.h"
#include "media/cast/video_receiver/codecs/vp8/vp8_decoder.h"
#include "media/cast/video_sender/codecs/vp8/vp8_encoder.h"

namespace media {
namespace cast {

namespace {
const int kWidth = 1280;
const int kHeight = 720;
const int kStartbitrate = 4000000;
const int kMaxQp = 54;
const int kMinQp = 4;
const int kMaxFrameRate = 30;
}  // namespace

class EncodeDecodeTest : public ::testing::Test {
 protected:
  EncodeDecodeTest() {
    encoder_config_.max_number_of_video_buffers_used = 1;
    encoder_config_.number_of_cores = 1;
    encoder_config_.width  = kWidth;
    encoder_config_.height  = kHeight;
    encoder_config_.start_bitrate = kStartbitrate;
    encoder_config_.min_qp = kMaxQp;
    encoder_config_.min_qp = kMinQp;
    encoder_config_.max_frame_rate = kMaxFrameRate;
    int max_unacked_frames = 1;
    encoder_.reset(new Vp8Encoder(encoder_config_, max_unacked_frames));
    // Initialize to use one core.
    decoder_.reset(new Vp8Decoder(1));
  }

  virtual void SetUp() {
    // Create test frame.
    int start_value = 10;  // Random value to start from.
    video_frame_.reset(new I420VideoFrame());
    video_frame_->width = encoder_config_.width;
    video_frame_->height = encoder_config_.height;
    PopulateVideoFrame(video_frame_.get(), start_value);
  }

  virtual void TearDown() {
    delete [] video_frame_->y_plane.data;
    delete [] video_frame_->u_plane.data;
    delete [] video_frame_->v_plane.data;
  }

  void Compare(const I420VideoFrame& original_image,
               const I420VideoFrame& decoded_image) {
    // Compare resolution.
    EXPECT_EQ(original_image.width, decoded_image.width);
    EXPECT_EQ(original_image.height, decoded_image.height);
    // Compare data.
    EXPECT_GT(I420PSNR(original_image, decoded_image), 40.0);
  }

  VideoSenderConfig encoder_config_;
  scoped_ptr<Vp8Encoder> encoder_;
  scoped_ptr<Vp8Decoder> decoder_;
  scoped_ptr<I420VideoFrame> video_frame_;
};

TEST_F(EncodeDecodeTest, BasicEncodeDecode) {
  EncodedVideoFrame encoded_frame;
  I420VideoFrame decoded_frame;
  // Encode frame.
  encoder_->Encode(*(video_frame_.get()), &encoded_frame);
  EXPECT_GT(encoded_frame.data.size(), GG_UINT64_C(0));
  // Decode frame.
  decoder_->Decode(encoded_frame, &decoded_frame);
  // Validate data.
  Compare(*(video_frame_.get()), decoded_frame);
}

}  // namespace cast
}  // namespace media
