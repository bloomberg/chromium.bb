// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/test/video_utility.h"
#include "media/cast/video_sender/video_encoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

namespace {
class TestVideoEncoderCallback :
    public base::RefCountedThreadSafe<TestVideoEncoderCallback>  {
 public:
  TestVideoEncoderCallback() {}

  void SetExpectedResult(bool expected_key_frame,
                         uint8 expected_frame_id,
                         uint8 expected_last_referenced_frame_id,
                         const base::TimeTicks& expected_capture_time) {
    expected_key_frame_ = expected_key_frame;
    expected_frame_id_ = expected_frame_id;
    expected_last_referenced_frame_id_ = expected_last_referenced_frame_id;
    expected_capture_time_ = expected_capture_time;
  }

  void DeliverEncodedVideoFrame(scoped_ptr<EncodedVideoFrame> encoded_frame,
                                const base::TimeTicks& capture_time) {
    EXPECT_EQ(expected_key_frame_, encoded_frame->key_frame);
    EXPECT_EQ(expected_frame_id_, encoded_frame->frame_id);
    EXPECT_EQ(expected_last_referenced_frame_id_,
              encoded_frame->last_referenced_frame_id);
    EXPECT_EQ(expected_capture_time_, capture_time);
  }

 protected:
  virtual ~TestVideoEncoderCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestVideoEncoderCallback>;

  bool expected_key_frame_;
  uint8 expected_frame_id_;
  uint8 expected_last_referenced_frame_id_;
  base::TimeTicks expected_capture_time_;
};
}  // namespace

class VideoEncoderTest : public ::testing::Test {
 protected:
  VideoEncoderTest()
      : test_video_encoder_callback_(new TestVideoEncoderCallback()) {
    video_config_.sender_ssrc = 1;
    video_config_.incoming_feedback_ssrc = 2;
    video_config_.rtp_payload_type = 127;
    video_config_.use_external_encoder = false;
    video_config_.width = 320;
    video_config_.height = 240;
    video_config_.max_bitrate = 5000000;
    video_config_.min_bitrate = 1000000;
    video_config_.start_bitrate = 2000000;
    video_config_.max_qp = 56;
    video_config_.min_qp = 0;
    video_config_.max_frame_rate = 30;
    video_config_.max_number_of_video_buffers_used = 3;
    video_config_.codec = kVp8;
    gfx::Size size(video_config_.width, video_config_.height);
    video_frame_ =  media::VideoFrame::CreateFrame(VideoFrame::I420,
        size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(video_frame_, 123);
  }

  virtual ~VideoEncoderTest() {}

  virtual void SetUp() {
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_environment_ = new CastEnvironment(&testing_clock_, task_runner_,
        task_runner_, task_runner_, task_runner_, task_runner_,
        GetDefaultCastLoggingConfig());
  }

  void Configure(uint8 max_unacked_frames) {
    video_encoder_.reset(new VideoEncoder(cast_environment_, video_config_,
       max_unacked_frames));
    video_encoder_controller_ = video_encoder_.get();
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<TestVideoEncoderCallback> test_video_encoder_callback_;
  VideoSenderConfig video_config_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_ptr<VideoEncoder> video_encoder_;
  VideoEncoderController* video_encoder_controller_;
  scoped_refptr<media::VideoFrame> video_frame_;

  scoped_refptr<CastEnvironment> cast_environment_;
};

TEST_F(VideoEncoderTest, EncodePattern30fpsRunningOutOfAck) {
  Configure(3);

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  base::TimeTicks capture_time;
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(true, 0, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
  frame_encoded_callback));
  task_runner_->RunTasks();

  capture_time += base::TimeDelta::FromMilliseconds(33);
  video_encoder_controller_->LatestFrameIdToReference(0);
  test_video_encoder_callback_->SetExpectedResult(false, 1, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
      frame_encoded_callback));
  task_runner_->RunTasks();

  capture_time += base::TimeDelta::FromMilliseconds(33);
  video_encoder_controller_->LatestFrameIdToReference(1);
  test_video_encoder_callback_->SetExpectedResult(false, 2, 1, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
      frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(2);

  for (int i = 3; i < 6; ++i) {
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, i, 2, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
        frame_encoded_callback));
    task_runner_->RunTasks();
  }
}

// TODO(pwestin): Re-enabled after redesign the encoder to control number of
// frames in flight.
TEST_F(VideoEncoderTest,DISABLED_EncodePattern60fpsRunningOutOfAck) {
  video_config_.max_number_of_video_buffers_used = 1;
  Configure(6);

  base::TimeTicks capture_time;
  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(true, 0, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
              frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(0);
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(false, 1, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
              frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(1);
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(false, 2, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
              frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(2);

  for (int i = 3; i < 9; ++i) {
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, i, 2, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
        frame_encoded_callback));
    task_runner_->RunTasks();
  }
}

// TODO(pwestin): Re-enabled after redesign the encoder to control number of
// frames in flight.
TEST_F(VideoEncoderTest, DISABLED_EncodePattern60fps200msDelayRunningOutOfAck) {
  Configure(12);

  base::TimeTicks capture_time;
  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(true, 0, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
              frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(0);
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(false, 1, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
              frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(1);
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(false, 2, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
              frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(2);
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(false, 3, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
        frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(3);
  capture_time += base::TimeDelta::FromMilliseconds(33);
  test_video_encoder_callback_->SetExpectedResult(false, 4, 0, capture_time);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
              frame_encoded_callback));
  task_runner_->RunTasks();

  video_encoder_controller_->LatestFrameIdToReference(4);

  for (int i = 5; i < 17; ++i) {
    test_video_encoder_callback_->SetExpectedResult(false, i, 4, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(video_frame_, capture_time,
        frame_encoded_callback));
    task_runner_->RunTasks();
  }
}

}  // namespace cast
}  // namespace media
