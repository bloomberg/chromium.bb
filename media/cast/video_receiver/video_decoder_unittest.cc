// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_thread.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/video_receiver/video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

// Random frame size for testing.
const int kFrameSize = 2345;

static void ReleaseFrame(const EncodedVideoFrame* encoded_frame) {
  // Empty since we in this test send in the same frame.
}

class TestVideoDecoderCallback :
    public base::RefCountedThreadSafe<TestVideoDecoderCallback> {
 public:
  TestVideoDecoderCallback()
      : num_called_(0) {}
  // TODO(mikhal): Set and check expectations.
  void DecodeComplete(scoped_ptr<I420VideoFrame> frame,
      const base::TimeTicks render_time) {
    num_called_++;
  }

  int number_times_called() {return num_called_;}
 private:
  int num_called_;
};

class VideoDecoderTest : public ::testing::Test {
 protected:
  VideoDecoderTest() {
    // Configure to vp8.
    config_.codec = kVp8;
    config_.use_external_decoder = false;
    video_decoder_callback_ = new TestVideoDecoderCallback();
  }

  ~VideoDecoderTest() {}
  virtual void SetUp() {
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_thread_ = new CastThread(task_runner_, task_runner_, task_runner_,
                                  task_runner_, task_runner_);
    decoder_ = new VideoDecoder(cast_thread_, config_);
  }

  scoped_refptr<VideoDecoder> decoder_;
  VideoReceiverConfig config_;
  EncodedVideoFrame encoded_frame_;
  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastThread> cast_thread_;
  scoped_refptr<TestVideoDecoderCallback> video_decoder_callback_;
};

// TODO(pwestin): Test decoding a real frame.
TEST_F(VideoDecoderTest, SizeZero) {
  encoded_frame_.codec = kVp8;
  base::TimeTicks render_time;
  VideoFrameDecodedCallback frame_decoded_callback =
      base::Bind(&TestVideoDecoderCallback::DecodeComplete,
                 video_decoder_callback_.get());
  decoder_->DecodeVideoFrame(&encoded_frame_, render_time,
      frame_decoded_callback, base::Bind(ReleaseFrame, &encoded_frame_));
  EXPECT_EQ(0, video_decoder_callback_->number_times_called());
}

TEST_F(VideoDecoderTest, InvalidCodec) {
  base::TimeTicks render_time;
  VideoFrameDecodedCallback frame_decoded_callback =
      base::Bind(&TestVideoDecoderCallback::DecodeComplete,
                 video_decoder_callback_.get());
  encoded_frame_.data.assign(kFrameSize, 0);
  encoded_frame_.codec = kExternalVideo;
  EXPECT_DEATH(decoder_->DecodeVideoFrame(&encoded_frame_, render_time,
      frame_decoded_callback, base::Bind(ReleaseFrame, &encoded_frame_)),
          "Invalid codec");
}

}  // namespace cast
}  // namespace media
