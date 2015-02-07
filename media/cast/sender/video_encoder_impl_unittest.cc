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
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class VideoEncoderImplTest : public ::testing::TestWithParam<Codec> {
 protected:
  VideoEncoderImplTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)),
        video_config_(GetDefaultVideoSenderConfig()),
        operational_status_(STATUS_UNINITIALIZED),
        count_frames_delivered_(0) {
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    first_frame_time_ = testing_clock_->NowTicks();
  }

  ~VideoEncoderImplTest() override {}

  void SetUp() override {
    video_config_.codec = GetParam();
  }

  void TearDown() override {
    video_encoder_.reset();
    task_runner_->RunTasks();
  }

  void CreateEncoder(bool three_buffer_mode) {
    ASSERT_EQ(STATUS_UNINITIALIZED, operational_status_);
    video_config_.max_number_of_video_buffers_used =
        (three_buffer_mode ? 3 : 1);
    video_encoder_.reset(new VideoEncoderImpl(
        cast_environment_,
        video_config_,
        base::Bind(&VideoEncoderImplTest::OnOperationalStatusChange,
                   base::Unretained(this))));
    task_runner_->RunTasks();
    ASSERT_EQ(STATUS_INITIALIZED, operational_status_);
  }

  VideoEncoder* video_encoder() const {
    return video_encoder_.get();
  }

  void AdvanceClock() {
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(33));
  }

  base::TimeTicks Now() const {
    return testing_clock_->NowTicks();
  }

  void RunTasks() const {
    return task_runner_->RunTasks();
  }

  int count_frames_delivered() const {
    return count_frames_delivered_;
  }

  // Return a callback that, when run, expects the EncodedFrame to have the
  // given properties.
  VideoEncoder::FrameEncodedCallback CreateFrameDeliverCallback(
      uint32 expected_frame_id,
      uint32 expected_last_referenced_frame_id,
      uint32 expected_rtp_timestamp,
      const base::TimeTicks& expected_reference_time) {
    return base::Bind(&VideoEncoderImplTest::DeliverEncodedVideoFrame,
                      base::Unretained(this),
                      expected_frame_id,
                      expected_last_referenced_frame_id,
                      expected_rtp_timestamp,
                      expected_reference_time);
  }

  // Creates a new VideoFrame of the given |size|, filled with a test pattern.
  scoped_refptr<media::VideoFrame> CreateTestVideoFrame(
      const gfx::Size& size) const {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size,
            testing_clock_->NowTicks() - first_frame_time_);
    PopulateVideoFrame(frame.get(), 123);
    return frame;
  }

 private:
  void OnOperationalStatusChange(OperationalStatus status) {
    operational_status_ = status;
  }

  // Checks that |encoded_frame| matches expected values.  This is the method
  // bound in the callback returned from CreateFrameDeliverCallback().
  void DeliverEncodedVideoFrame(
      uint32 expected_frame_id,
      uint32 expected_last_referenced_frame_id,
      uint32 expected_rtp_timestamp,
      const base::TimeTicks& expected_reference_time,
      scoped_ptr<EncodedFrame> encoded_frame) {
    if (expected_frame_id != expected_last_referenced_frame_id) {
      EXPECT_EQ(EncodedFrame::DEPENDENT, encoded_frame->dependency);
    } else if (video_config_.max_number_of_video_buffers_used == 1) {
      EXPECT_EQ(EncodedFrame::KEY, encoded_frame->dependency);
    }
    EXPECT_EQ(expected_frame_id, encoded_frame->frame_id);
    EXPECT_EQ(expected_last_referenced_frame_id,
              encoded_frame->referenced_frame_id)
        << "frame id: " << expected_frame_id;
    EXPECT_EQ(expected_rtp_timestamp, encoded_frame->rtp_timestamp);
    EXPECT_EQ(expected_reference_time, encoded_frame->reference_time);
    EXPECT_FALSE(encoded_frame->data.empty());
    ++count_frames_delivered_;
  }

  base::SimpleTestTickClock* const testing_clock_;  // Owned by CastEnvironment.
  const scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  const scoped_refptr<CastEnvironment> cast_environment_;
  VideoSenderConfig video_config_;
  base::TimeTicks first_frame_time_;
  OperationalStatus operational_status_;
  scoped_ptr<VideoEncoder> video_encoder_;

  int count_frames_delivered_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderImplTest);
};

// A simple test to encode ten frames of video, expecting to see one key frame
// followed by nine delta frames.
TEST_P(VideoEncoderImplTest, GeneratesKeyFrameThenOnlyDeltaFrames) {
  CreateEncoder(false);

  EXPECT_EQ(0, count_frames_delivered());

  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestVideoFrame(gfx::Size(1280, 720));
  EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
      video_frame,
      Now(),
      CreateFrameDeliverCallback(
          0, 0, TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
          Now())));
  RunTasks();

  for (uint32 frame_id = 1; frame_id < 10; ++frame_id) {
    AdvanceClock();
    video_frame = CreateTestVideoFrame(gfx::Size(1280, 720));
    EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
        video_frame,
        Now(),
        CreateFrameDeliverCallback(
            frame_id, frame_id - 1,
            TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
            Now())));
    RunTasks();
  }

  EXPECT_EQ(10, count_frames_delivered());
}

// Tests basic frame dependency rules when using the VP8 encoder in multi-buffer
// mode.
TEST_P(VideoEncoderImplTest,
       FramesDoNotDependOnUnackedFramesInMultiBufferMode) {
  if (GetParam() != CODEC_VIDEO_VP8)
    return;  // Only test multibuffer mode for the VP8 encoder.
  CreateEncoder(true);

  EXPECT_EQ(0, count_frames_delivered());

  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestVideoFrame(gfx::Size(1280, 720));
  EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
      video_frame,
      Now(),
      CreateFrameDeliverCallback(
          0, 0, TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
          Now())));
  RunTasks();

  AdvanceClock();
  video_encoder()->LatestFrameIdToReference(0);
  video_frame = CreateTestVideoFrame(gfx::Size(1280, 720));
  EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
      video_frame,
      Now(),
      CreateFrameDeliverCallback(
          1, 0, TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
          Now())));
  RunTasks();

  AdvanceClock();
  video_encoder()->LatestFrameIdToReference(1);
  video_frame = CreateTestVideoFrame(gfx::Size(1280, 720));
  EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
      video_frame,
      Now(),
      CreateFrameDeliverCallback(
          2, 1, TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
          Now())));
  RunTasks();

  video_encoder()->LatestFrameIdToReference(2);

  for (uint32 frame_id = 3; frame_id < 10; ++frame_id) {
    AdvanceClock();
    video_frame = CreateTestVideoFrame(gfx::Size(1280, 720));
    EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
        video_frame,
        Now(),
        CreateFrameDeliverCallback(
            frame_id, 2,
            TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
            Now())));
    RunTasks();
  }

  EXPECT_EQ(10, count_frames_delivered());
}

// Tests that the encoder continues to output EncodedFrames as the frame size
// changes.  See media/cast/receiver/video_decoder_unittest.cc for a complete
// encode/decode cycle of varied frame sizes that actually checks the frame
// content.
TEST_P(VideoEncoderImplTest, EncodesVariedFrameSizes) {
  CreateEncoder(false);
  ASSERT_TRUE(video_encoder()->CanEncodeVariedFrameSizes());

  EXPECT_EQ(0, count_frames_delivered());

  std::vector<gfx::Size> frame_sizes;
  frame_sizes.push_back(gfx::Size(1280, 720));
  frame_sizes.push_back(gfx::Size(640, 360));  // Shrink both dimensions.
  frame_sizes.push_back(gfx::Size(300, 200));  // Shrink both dimensions again.
  frame_sizes.push_back(gfx::Size(200, 300));  // Same area.
  frame_sizes.push_back(gfx::Size(600, 400));  // Grow both dimensions.
  frame_sizes.push_back(gfx::Size(638, 400));  // Shrink only one dimension.
  frame_sizes.push_back(gfx::Size(638, 398));  // Shrink the other dimension.
  frame_sizes.push_back(gfx::Size(320, 180));  // Shrink both dimensions again.
  frame_sizes.push_back(gfx::Size(322, 180));  // Grow only one dimension.
  frame_sizes.push_back(gfx::Size(322, 182));  // Grow the other dimension.
  frame_sizes.push_back(gfx::Size(1920, 1080));  // Grow both dimensions again.

  uint32 frame_id = 0;

  // Encode one frame at each size.  Expect nothing but key frames to come out.
  for (const auto& frame_size : frame_sizes) {
    AdvanceClock();
    const scoped_refptr<media::VideoFrame> video_frame =
        CreateTestVideoFrame(frame_size);
    EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
        video_frame,
        Now(),
        CreateFrameDeliverCallback(
            frame_id,
            frame_id,
            TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
            Now())));
    RunTasks();
    ++frame_id;
  }

  // Encode 10 frames at each size.  Expect one key frame followed by nine delta
  // frames for each frame size.
  for (const auto& frame_size : frame_sizes) {
    for (int i = 0; i < 10; ++i) {
      AdvanceClock();
      const scoped_refptr<media::VideoFrame> video_frame =
          CreateTestVideoFrame(frame_size);
      EXPECT_TRUE(video_encoder()->EncodeVideoFrame(
          video_frame,
          Now(),
          CreateFrameDeliverCallback(
              frame_id,
              i == 0 ? frame_id : frame_id - 1,
              TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
              Now())));
      RunTasks();
      ++frame_id;
    }
  }

  EXPECT_EQ(static_cast<int>(frame_id), count_frames_delivered());
}

INSTANTIATE_TEST_CASE_P(,
                        VideoEncoderImplTest,
                        ::testing::Values(CODEC_VIDEO_FAKE, CODEC_VIDEO_VP8));

}  // namespace cast
}  // namespace media
