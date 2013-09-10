// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_thread.h"
#include "media/cast/video_sender/video_encoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using base::RunLoop;
using base::MessageLoopProxy;
using base::Thread;
using testing::_;

static void ReleaseFrame(const I420VideoFrame* frame) {
  // Empty since we in this test send in the same frame.
};

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

 private:
  bool expected_key_frame_;
  uint8 expected_frame_id_;
  uint8 expected_last_referenced_frame_id_;
  base::TimeTicks expected_capture_time_;
};

class VideoEncoderTest : public ::testing::Test {
 public:

 protected:
  VideoEncoderTest()
      : pixels_(320 * 240, 123),
        test_video_encoder_callback_(new TestVideoEncoderCallback()) {
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
    video_frame_.width = 320;
    video_frame_.height = 240;
    video_frame_.y_plane.stride = video_frame_.width;
    video_frame_.y_plane.length = video_frame_.width;
    video_frame_.y_plane.data = &(pixels_[0]);
    video_frame_.u_plane.stride = video_frame_.width / 2;
    video_frame_.u_plane.length = video_frame_.width / 2;
    video_frame_.u_plane.data = &(pixels_[0]);
    video_frame_.v_plane.stride = video_frame_.width / 2;
    video_frame_.v_plane.length = video_frame_.width / 2;
    video_frame_.v_plane.data = &(pixels_[0]);
  }

  ~VideoEncoderTest() {}

  virtual void SetUp() {
    cast_thread_ = new CastThread(MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current());
  }

  void Configure(uint8 max_unacked_frames) {
    video_encoder_= new VideoEncoder(cast_thread_, video_config_,
       max_unacked_frames);
    video_encoder_controller_ = video_encoder_.get();
  }

  base::MessageLoop loop_;
  std::vector<uint8> pixels_;
  scoped_refptr<TestVideoEncoderCallback> test_video_encoder_callback_;
  VideoSenderConfig video_config_;
  scoped_refptr<VideoEncoder> video_encoder_;
  VideoEncoderController* video_encoder_controller_;
  I420VideoFrame video_frame_;

  scoped_refptr<CastThread> cast_thread_;
};

TEST_F(VideoEncoderTest, EncodePattern30fpsRunningOutOfAck) {
  Configure(3);

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  base::TimeTicks capture_time;
  {
    RunLoop run_loop;
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(true, 0, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
    frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    capture_time += base::TimeDelta::FromMilliseconds(33);
    video_encoder_controller_->LatestFrameIdToReference(0);
    test_video_encoder_callback_->SetExpectedResult(false, 1, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    capture_time += base::TimeDelta::FromMilliseconds(33);
    video_encoder_controller_->LatestFrameIdToReference(1);
    test_video_encoder_callback_->SetExpectedResult(false, 2, 1, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }

  video_encoder_controller_->LatestFrameIdToReference(2);

  for (int i = 3; i < 6; ++i) {
    RunLoop run_loop;
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, i, 2, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
}

TEST_F(VideoEncoderTest, EncodePattern60fpsRunningOutOfAck) {
  Configure(6);

  base::TimeTicks capture_time;
  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  {
    RunLoop run_loop;
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(true, 0, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
      frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    video_encoder_controller_->LatestFrameIdToReference(0);
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, 1, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    video_encoder_controller_->LatestFrameIdToReference(1);
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, 2, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }

  video_encoder_controller_->LatestFrameIdToReference(2);

  for (int i = 3; i < 9; ++i) {
    RunLoop run_loop;
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, i, 2, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame( &video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
}

TEST_F(VideoEncoderTest, EncodePattern60fps200msDelayRunningOutOfAck) {
  Configure(12);

  base::TimeTicks capture_time;
  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  {
    RunLoop run_loop;
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(true, 0, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    video_encoder_controller_->LatestFrameIdToReference(0);
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, 1, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
      frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    video_encoder_controller_->LatestFrameIdToReference(1);
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, 2, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    video_encoder_controller_->LatestFrameIdToReference(2);
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, 3, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
  {
    RunLoop run_loop;
    video_encoder_controller_->LatestFrameIdToReference(3);
    capture_time += base::TimeDelta::FromMilliseconds(33);
    test_video_encoder_callback_->SetExpectedResult(false, 4, 0, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }

  video_encoder_controller_->LatestFrameIdToReference(4);

  for (int i = 5; i < 17; ++i) {
    RunLoop run_loop;
    test_video_encoder_callback_->SetExpectedResult(false, i, 4, capture_time);
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(&video_frame_, capture_time,
        frame_encoded_callback, base::Bind(ReleaseFrame, &video_frame_)));
    run_loop.RunUntilIdle();
  }
}

}  // namespace cast
}  // namespace media
