// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Joint encoder and decoder testing.
// These tests operate directly on the VP8 encoder and decoder, not the
// transport layer, and are targeted at validating the bit stream.

#include <gtest/gtest.h>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/test/video_utility.h"
#include "media/cast/video_receiver/codecs/vp8/vp8_decoder.h"
#include "media/cast/video_sender/codecs/vp8/vp8_encoder.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = GG_INT64_C(1245);
static const int kWidth = 1280;
static const int kHeight = 720;
static const int kStartbitrate = 4000000;
static const int kMaxQp = 54;
static const int kMinQp = 4;
static const int kMaxFrameRate = 30;

namespace {
class EncodeDecodeTestFrameCallback :
    public base::RefCountedThreadSafe<EncodeDecodeTestFrameCallback> {
 public:
  EncodeDecodeTestFrameCallback()
      : num_called_(0) {
  gfx::Size size(kWidth, kHeight);
  original_frame_ = media::VideoFrame::CreateFrame(
      VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
  }

  void SetFrameStartValue(int start_value) {
    PopulateVideoFrame(original_frame_.get(), start_value);
  }

  void DecodeComplete(const scoped_refptr<media::VideoFrame>& decoded_frame,
                      const base::TimeTicks& render_time) {
    ++num_called_;
    // Compare resolution.
    EXPECT_EQ(original_frame_->coded_size().width(),
              decoded_frame->coded_size().width());
    EXPECT_EQ(original_frame_->coded_size().height(),
              decoded_frame->coded_size().height());
    // Compare data.
    EXPECT_GT(I420PSNR(original_frame_, decoded_frame), 40.0);
  }

  int num_called() const {
    return num_called_;
  }

 protected:
  virtual ~EncodeDecodeTestFrameCallback() {}

 private:
  friend class base::RefCountedThreadSafe<EncodeDecodeTestFrameCallback>;

  int num_called_;
  scoped_refptr<media::VideoFrame> original_frame_;
};
}  // namespace

class EncodeDecodeTest : public ::testing::Test {
 protected:
  EncodeDecodeTest()
      : task_runner_(new test::FakeTaskRunner(&testing_clock_)),
        // CastEnvironment will only be used by the vp8 decoder; Enable only the
        // video decoder and main threads.
        cast_environment_(new CastEnvironment(&testing_clock_, task_runner_,
            NULL, NULL, NULL, task_runner_, GetDefaultCastLoggingConfig())),
        test_callback_(new EncodeDecodeTestFrameCallback()) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
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
    decoder_.reset(new Vp8Decoder(cast_environment_));
  }

  virtual ~EncodeDecodeTest() {}

  virtual void SetUp() OVERRIDE {
    // Create test frame.
    int start_value = 10;  // Random value to start from.
    gfx::Size size(encoder_config_.width, encoder_config_.height);
    video_frame_ =  media::VideoFrame::CreateFrame(VideoFrame::I420,
        size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(video_frame_, start_value);
    test_callback_->SetFrameStartValue(start_value);
  }

  VideoSenderConfig encoder_config_;
  scoped_ptr<Vp8Encoder> encoder_;
  scoped_ptr<Vp8Decoder> decoder_;
  scoped_refptr<media::VideoFrame> video_frame_;
  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<EncodeDecodeTestFrameCallback> test_callback_;
};

TEST_F(EncodeDecodeTest, BasicEncodeDecode) {
  EncodedVideoFrame encoded_frame;
  // Encode frame.
  encoder_->Encode(video_frame_, &encoded_frame);
  EXPECT_GT(encoded_frame.data.size(), GG_UINT64_C(0));
  // Decode frame.
  decoder_->Decode(&encoded_frame, base::TimeTicks(), base::Bind(
      &EncodeDecodeTestFrameCallback::DecodeComplete, test_callback_));
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media
