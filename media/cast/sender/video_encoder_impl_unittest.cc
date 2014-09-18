// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/sender/video_encoder_impl.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

namespace {
class TestVideoEncoderCallback
    : public base::RefCountedThreadSafe<TestVideoEncoderCallback> {
 public:
  explicit TestVideoEncoderCallback(bool multiple_buffer_mode)
      : multiple_buffer_mode_(multiple_buffer_mode),
        count_frames_delivered_(0) {}

  int count_frames_delivered() const {
    return count_frames_delivered_;
  }

  void SetExpectedResult(uint32 expected_frame_id,
                         uint32 expected_last_referenced_frame_id,
                         const base::TimeTicks& expected_capture_time) {
    expected_frame_id_ = expected_frame_id;
    expected_last_referenced_frame_id_ = expected_last_referenced_frame_id;
    expected_capture_time_ = expected_capture_time;
  }

  void DeliverEncodedVideoFrame(
      scoped_ptr<EncodedFrame> encoded_frame) {
    if (expected_frame_id_ != expected_last_referenced_frame_id_) {
      EXPECT_EQ(EncodedFrame::DEPENDENT, encoded_frame->dependency);
    } else if (!multiple_buffer_mode_) {
      EXPECT_EQ(EncodedFrame::KEY, encoded_frame->dependency);
    }
    EXPECT_EQ(expected_frame_id_, encoded_frame->frame_id);
    EXPECT_EQ(expected_last_referenced_frame_id_,
              encoded_frame->referenced_frame_id)
        << "frame id: " << expected_frame_id_;
    EXPECT_LT(0u, encoded_frame->rtp_timestamp);
    EXPECT_EQ(expected_capture_time_, encoded_frame->reference_time);
    EXPECT_FALSE(encoded_frame->data.empty());
    ++count_frames_delivered_;
  }

 private:
  friend class base::RefCountedThreadSafe<TestVideoEncoderCallback>;
  virtual ~TestVideoEncoderCallback() {}

  const bool multiple_buffer_mode_;
  int count_frames_delivered_;

  uint32 expected_frame_id_;
  uint32 expected_last_referenced_frame_id_;
  base::TimeTicks expected_capture_time_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoEncoderCallback);
};
}  // namespace

class VideoEncoderImplTest : public ::testing::Test {
 protected:
  VideoEncoderImplTest() {
    video_config_ = GetDefaultVideoSenderConfig();
    video_config_.codec = CODEC_VIDEO_VP8;
    gfx::Size size(video_config_.width, video_config_.height);
    video_frame_ = media::VideoFrame::CreateFrame(
        VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(video_frame_.get(), 123);
  }

  virtual ~VideoEncoderImplTest() {}

  virtual void SetUp() OVERRIDE {
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);
  }

  virtual void TearDown() OVERRIDE {
    video_encoder_.reset();
    task_runner_->RunTasks();
  }

  void CreateEncoder() {
    test_video_encoder_callback_ = new TestVideoEncoderCallback(
        video_config_.max_number_of_video_buffers_used != 1);
    video_encoder_.reset(new VideoEncoderImpl(
        cast_environment_, video_config_,
        0 /* useless arg to be removed in later change */));
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<TestVideoEncoderCallback> test_video_encoder_callback_;
  VideoSenderConfig video_config_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<VideoEncoder> video_encoder_;
  scoped_refptr<media::VideoFrame> video_frame_;

  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderImplTest);
};

TEST_F(VideoEncoderImplTest, GeneratesKeyFrameThenOnlyDeltaFrames) {
  CreateEncoder();

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  EXPECT_EQ(0, test_video_encoder_callback_->count_frames_delivered());

  test_video_encoder_callback_->SetExpectedResult(
      0, 0, testing_clock_->NowTicks());
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
  task_runner_->RunTasks();

  for (uint32 frame_id = 1; frame_id < 10; ++frame_id) {
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(33));
    test_video_encoder_callback_->SetExpectedResult(
        frame_id, frame_id - 1, testing_clock_->NowTicks());
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
        video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
    task_runner_->RunTasks();
  }

  EXPECT_EQ(10, test_video_encoder_callback_->count_frames_delivered());
}

TEST_F(VideoEncoderImplTest,
       FramesDoNotDependOnUnackedFramesInMultiBufferMode) {
  video_config_.max_number_of_video_buffers_used = 3;
  CreateEncoder();

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  EXPECT_EQ(0, test_video_encoder_callback_->count_frames_delivered());

  test_video_encoder_callback_->SetExpectedResult(
      0, 0, testing_clock_->NowTicks());
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
  task_runner_->RunTasks();

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(33));
  video_encoder_->LatestFrameIdToReference(0);
  test_video_encoder_callback_->SetExpectedResult(
      1, 0, testing_clock_->NowTicks());
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
  task_runner_->RunTasks();

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(33));
  video_encoder_->LatestFrameIdToReference(1);
  test_video_encoder_callback_->SetExpectedResult(
      2, 1, testing_clock_->NowTicks());
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_->LatestFrameIdToReference(2);

  for (uint32 frame_id = 3; frame_id < 10; ++frame_id) {
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(33));
    test_video_encoder_callback_->SetExpectedResult(
        frame_id, 2, testing_clock_->NowTicks());
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
        video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
    task_runner_->RunTasks();
  }

  EXPECT_EQ(10, test_video_encoder_callback_->count_frames_delivered());
}

}  // namespace cast
}  // namespace media
