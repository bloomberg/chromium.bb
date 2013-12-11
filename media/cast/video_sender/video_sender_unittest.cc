// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/pacing/mock_paced_packet_sender.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/test/video_utility.h"
#include "media/cast/video_sender/mock_video_encoder_controller.h"
#include "media/cast/video_sender/video_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {
static const int64 kStartMillisecond = GG_INT64_C(12345678900000);
static const uint8 kPixelValue = 123;
static const int kWidth = 320;
static const int kHeight = 240;
}

using testing::_;
using testing::AtLeast;

namespace {
class PeerVideoSender : public VideoSender {
 public:
  PeerVideoSender(scoped_refptr<CastEnvironment> cast_environment,
                  const VideoSenderConfig& video_config,
                  VideoEncoderController* const video_encoder_controller,
                  PacedPacketSender* const paced_packet_sender)
      : VideoSender(cast_environment, video_config,
                    video_encoder_controller, paced_packet_sender) {
  }
  using VideoSender::OnReceivedCastFeedback;
};
}  // namespace

class VideoSenderTest : public ::testing::Test {
 protected:
  VideoSenderTest() {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual ~VideoSenderTest() {}

  void InitEncoder(bool external) {
    VideoSenderConfig video_config;
    video_config.sender_ssrc = 1;
    video_config.incoming_feedback_ssrc = 2;
    video_config.rtp_payload_type = 127;
    video_config.use_external_encoder = external;
    video_config.width = kWidth;
    video_config.height = kHeight;
    video_config.max_bitrate = 5000000;
    video_config.min_bitrate = 1000000;
    video_config.start_bitrate = 1000000;
    video_config.max_qp = 56;
    video_config.min_qp = 0;
    video_config.max_frame_rate = 30;
    video_config.max_number_of_video_buffers_used = 1;
    video_config.codec = kVp8;

    if (external) {
      video_sender_.reset(new PeerVideoSender(cast_environment_,
          video_config, &mock_video_encoder_controller_, &mock_transport_));
    } else {
      video_sender_.reset(new PeerVideoSender(cast_environment_, video_config,
                                              NULL, &mock_transport_));
    }
  }

  virtual void SetUp() {
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_environment_ = new CastEnvironment(&testing_clock_, task_runner_,
       task_runner_, task_runner_, task_runner_, task_runner_,
       GetDefaultCastLoggingConfig());
  }

  scoped_refptr<media::VideoFrame> GetNewVideoFrame() {
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(VideoFrame::I420, size, gfx::Rect(size),
                                       size, base::TimeDelta());
    PopulateVideoFrame(video_frame, kPixelValue);
    return video_frame;
  }

  MockVideoEncoderController mock_video_encoder_controller_;
  base::SimpleTestTickClock testing_clock_;
  MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_ptr<PeerVideoSender> video_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

TEST_F(VideoSenderTest, BuiltInEncoder) {
  EXPECT_CALL(mock_transport_, SendPackets(_)).Times(1);

  InitEncoder(false);
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, ExternalEncoder) {
  EXPECT_CALL(mock_transport_, SendPackets(_)).Times(1);
  EXPECT_CALL(mock_video_encoder_controller_, SkipNextFrame(false)).Times(1);
  InitEncoder(true);

  EncodedVideoFrame video_frame;
  base::TimeTicks capture_time;

  video_frame.codec = kVp8;
  video_frame.key_frame = true;
  video_frame.frame_id = 0;
  video_frame.last_referenced_frame_id = 0;
  video_frame.data.insert(video_frame.data.begin(), 1000, kPixelValue);

  video_sender_->InsertCodedVideoFrame(&video_frame, capture_time,
    base::Bind(base::DoNothing));
}

TEST_F(VideoSenderTest, RtcpTimer) {
  EXPECT_CALL(mock_transport_, SendPackets(_)).Times(AtLeast(1));
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).Times(1);
  EXPECT_CALL(mock_video_encoder_controller_,
              SkipNextFrame(false)).Times(AtLeast(1));
  InitEncoder(true);

  EncodedVideoFrame video_frame;
  base::TimeTicks capture_time;

  video_frame.codec = kVp8;
  video_frame.key_frame = true;
  video_frame.frame_id = 0;
  video_frame.last_referenced_frame_id = 0;
  video_frame.data.insert(video_frame.data.begin(), 1000, kPixelValue);

  video_sender_->InsertCodedVideoFrame(&video_frame, capture_time,
    base::Bind(base::DoNothing));

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);

  testing_clock_.Advance(max_rtcp_timeout);
  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, ResendTimer) {
  EXPECT_CALL(mock_transport_, SendPackets(_)).Times(2);
  EXPECT_CALL(mock_transport_, ResendPackets(_)).Times(1);

  InitEncoder(false);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  task_runner_->RunTasks();

  // ACK the key frame.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  task_runner_->RunTasks();

  base::TimeDelta max_resend_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtpMaxDelayMs);

  // Make sure that we do a re-send.
  testing_clock_.Advance(max_resend_timeout);
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media

