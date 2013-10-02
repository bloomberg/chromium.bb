// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_thread.h"
#include "media/cast/pacing/mock_paced_packet_sender.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/video_sender/mock_video_encoder_controller.h"
#include "media/cast/video_sender/video_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = 123456789;

using testing::_;

class PeerVideoSender : public VideoSender {
 public:
  PeerVideoSender(scoped_refptr<CastThread> cast_thread,
                  const VideoSenderConfig& video_config,
                  VideoEncoderController* const video_encoder_controller,
                  PacedPacketSender* const paced_packet_sender)
      : VideoSender(cast_thread, video_config, video_encoder_controller,
                    paced_packet_sender) {
  }
  using VideoSender::OnReceivedCastFeedback;
};

static void ReleaseVideoFrame(const I420VideoFrame* frame) {
  delete [] frame->y_plane.data;
  delete [] frame->u_plane.data;
  delete [] frame->v_plane.data;
  delete frame;
}

static void ReleaseEncodedFrame(const EncodedVideoFrame* frame) {
  // Do nothing.
}

class VideoSenderTest : public ::testing::Test {
 protected:
  VideoSenderTest() {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  ~VideoSenderTest() {}

  void InitEncoder(bool external) {
    VideoSenderConfig video_config;
    video_config.sender_ssrc = 1;
    video_config.incoming_feedback_ssrc = 2;
    video_config.rtp_payload_type = 127;
    video_config.use_external_encoder = external;
    video_config.width = 320;
    video_config.height = 240;
    video_config.max_bitrate = 5000000;
    video_config.min_bitrate = 1000000;
    video_config.start_bitrate = 1000000;
    video_config.max_qp = 56;
    video_config.min_qp = 0;
    video_config.max_frame_rate = 30;
    video_config.max_number_of_video_buffers_used = 3;
    video_config.codec = kVp8;

    if (external) {
      video_sender_.reset(new PeerVideoSender(cast_thread_, video_config,
          &mock_video_encoder_controller_, &mock_transport_));
    } else {
      video_sender_.reset(new PeerVideoSender(cast_thread_, video_config, NULL,
          &mock_transport_));
    }
    video_sender_->set_clock(&testing_clock_);
  }

  virtual void SetUp() {
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_thread_ = new CastThread(task_runner_, task_runner_, task_runner_,
                                  task_runner_, task_runner_);
  }

  I420VideoFrame* AllocateNewVideoFrame() {
    I420VideoFrame* video_frame = new I420VideoFrame();
    video_frame->width = 320;
    video_frame->height = 240;

    video_frame->y_plane.stride = video_frame->width;
    video_frame->y_plane.length = video_frame->width;
    video_frame->y_plane.data =
        new uint8[video_frame->width * video_frame->height];
    memset(video_frame->y_plane.data, 123,
        video_frame->width * video_frame->height);
    video_frame->u_plane.stride = video_frame->width / 2;
    video_frame->u_plane.length = video_frame->width / 2;
    video_frame->u_plane.data =
        new uint8[video_frame->width * video_frame->height / 4];
    memset(video_frame->u_plane.data, 123,
        video_frame->width * video_frame->height / 4);
    video_frame->v_plane.stride = video_frame->width / 2;
    video_frame->v_plane.length = video_frame->width / 2;
    video_frame->v_plane.data =
        new uint8[video_frame->width * video_frame->height / 4];
    memset(video_frame->v_plane.data, 123,
        video_frame->width * video_frame->height / 4);
    return video_frame;
  }

  MockVideoEncoderController mock_video_encoder_controller_;
  base::SimpleTestTickClock testing_clock_;
  MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_ptr<PeerVideoSender> video_sender_;
  scoped_refptr<CastThread> cast_thread_;
};

TEST_F(VideoSenderTest, BuiltInEncoder) {
  EXPECT_CALL(mock_transport_, SendPacket(_, _)).Times(1);

  InitEncoder(false);
  I420VideoFrame* video_frame = AllocateNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time,
      base::Bind(&ReleaseVideoFrame, video_frame));

  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, ExternalEncoder) {
  EXPECT_CALL(mock_transport_, SendPacket(_, _)).Times(1);
  EXPECT_CALL(mock_video_encoder_controller_, SkipNextFrame(false)).Times(1);
  InitEncoder(true);

  EncodedVideoFrame video_frame;
  base::TimeTicks capture_time;

  video_frame.codec = kVp8;
  video_frame.key_frame = true;
  video_frame.frame_id = 0;
  video_frame.last_referenced_frame_id = 0;
  video_frame.data.insert(video_frame.data.begin(), 123, 1000);

  video_sender_->InsertCodedVideoFrame(&video_frame, capture_time,
    base::Bind(&ReleaseEncodedFrame, &video_frame));
}

TEST_F(VideoSenderTest, RtcpTimer) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).Times(1);
  InitEncoder(false);

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);

  testing_clock_.Advance(max_rtcp_timeout);
  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, ResendTimer) {
  EXPECT_CALL(mock_transport_, SendPacket(_, _)).Times(2);
  EXPECT_CALL(mock_transport_, ResendPacket(_, _)).Times(1);

  InitEncoder(false);

  I420VideoFrame* video_frame = AllocateNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time,
      base::Bind(&ReleaseVideoFrame, video_frame));

  task_runner_->RunTasks();

  // ACK the key frame.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  video_frame = AllocateNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time,
      base::Bind(&ReleaseVideoFrame, video_frame));

  task_runner_->RunTasks();

  base::TimeDelta max_resend_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtpMaxDelayMs);

  // Make sure that we do a re-send.
  testing_clock_.Advance(max_resend_timeout);
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media

