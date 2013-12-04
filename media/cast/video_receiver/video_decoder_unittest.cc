// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/video_receiver/video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

// Random frame size for testing.
const int kFrameSize = 2345;
static const int64 kStartMillisecond = GG_INT64_C(1245);

namespace {
class DecodeTestFrameCallback :
  public base::RefCountedThreadSafe<DecodeTestFrameCallback> {
 public:
  DecodeTestFrameCallback() {}

  void DecodeComplete(const scoped_refptr<media::VideoFrame>& decoded_frame,
                      const base::TimeTicks& render_time) {}
 protected:
  virtual ~DecodeTestFrameCallback() {}
 private:
  friend class base::RefCountedThreadSafe<DecodeTestFrameCallback>;
};
}  // namespace

class VideoDecoderTest : public ::testing::Test {
 protected:
  VideoDecoderTest()
    : task_runner_(new test::FakeTaskRunner(&testing_clock_)),
      cast_environment_(new CastEnvironment(&testing_clock_, task_runner_,
          task_runner_, task_runner_, task_runner_, task_runner_,
          GetDefaultCastLoggingConfig())),
      test_callback_(new DecodeTestFrameCallback()) {
    // Configure to vp8.
    config_.codec = kVp8;
    config_.use_external_decoder = false;
    decoder_.reset(new VideoDecoder(config_, cast_environment_));
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual ~VideoDecoderTest() {}

  scoped_ptr<VideoDecoder> decoder_;
  VideoReceiverConfig config_;
  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<DecodeTestFrameCallback> test_callback_;
};

// TODO(pwestin): EXPECT_DEATH tests can not pass valgrind.
TEST_F(VideoDecoderTest, DISABLED_SizeZero) {
  EncodedVideoFrame encoded_frame;
  base::TimeTicks render_time;
  encoded_frame.codec = kVp8;
  EXPECT_DEATH(
      decoder_->DecodeVideoFrame(
          &encoded_frame, render_time,
          base::Bind(&DecodeTestFrameCallback::DecodeComplete, test_callback_)),
      "Empty frame");
}

// TODO(pwestin): EXPECT_DEATH tests can not pass valgrind.
TEST_F(VideoDecoderTest, DISABLED_InvalidCodec) {
  EncodedVideoFrame encoded_frame;
  base::TimeTicks render_time;
  encoded_frame.data.assign(kFrameSize, 0);
  encoded_frame.codec = kExternalVideo;
  EXPECT_DEATH(
      decoder_->DecodeVideoFrame(&encoded_frame, render_time, base::Bind(
          &DecodeTestFrameCallback::DecodeComplete, test_callback_)),
      "Invalid codec");
}

// TODO(pwestin): Test decoding a real frame.

}  // namespace cast
}  // namespace media
