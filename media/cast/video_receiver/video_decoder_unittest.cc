// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_defines.h"
#include "media/cast/video_receiver/video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

// Random frame size for testing.
const int kFrameSize = 2345;

class VideoDecoderTest : public ::testing::Test {
 protected:
  VideoDecoderTest() {
    // Configure to vp8.
    config_.codec = kVp8;
    config_.use_external_decoder = false;
    decoder_.reset(new VideoDecoder(config_));
  }

  virtual ~VideoDecoderTest() {}

  scoped_ptr<VideoDecoder> decoder_;
  VideoReceiverConfig config_;
};

// TODO(pwestin): EXPECT_DEATH tests can not pass valgrind.
TEST_F(VideoDecoderTest, DISABLED_SizeZero) {
  EncodedVideoFrame encoded_frame;
  I420VideoFrame video_frame;
  base::TimeTicks render_time;
  encoded_frame.codec = kVp8;

  EXPECT_DEATH(
      decoder_->DecodeVideoFrame(&encoded_frame, render_time, &video_frame),
     "Empty video frame");
}

// TODO(pwestin): EXPECT_DEATH tests can not pass valgrind.
TEST_F(VideoDecoderTest, DISABLED_InvalidCodec) {
  EncodedVideoFrame encoded_frame;
  I420VideoFrame video_frame;
  base::TimeTicks render_time;
  encoded_frame.data.assign(kFrameSize, 0);
  encoded_frame.codec = kExternalVideo;
  EXPECT_DEATH(
      decoder_->DecodeVideoFrame(&encoded_frame, render_time, &video_frame),
     "Invalid codec");
}

// TODO(pwestin): Test decoding a real frame.

}  // namespace cast
}  // namespace media
