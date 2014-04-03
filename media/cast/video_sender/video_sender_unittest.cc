// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/fake_video_encode_accelerator.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender_impl.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "media/cast/video_sender/video_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {
static const int64 kStartMillisecond = INT64_C(12345678900000);
static const uint8 kPixelValue = 123;
static const int kWidth = 320;
static const int kHeight = 240;

using testing::_;
using testing::AtLeast;

void CreateVideoEncodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_ptr<VideoEncodeAccelerator> fake_vea,
    const ReceiveVideoEncodeAcceleratorCallback& callback) {
  callback.Run(task_runner, fake_vea.Pass());
}

void CreateSharedMemory(
    size_t size, const ReceiveVideoEncodeMemoryCallback& callback) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAndMapAnonymous(size)) {
    NOTREACHED();
    return;
  }
  callback.Run(shm.Pass());
}

class TestPacketSender : public transport::PacketSender {
 public:
  TestPacketSender() : number_of_rtp_packets_(0), number_of_rtcp_packets_(0) {}

  // A singular packet implies a RTCP packet.
  virtual bool SendPacket(const Packet& packet) OVERRIDE {
    if (Rtcp::IsRtcpPacket(&packet[0], packet.size())) {
      ++number_of_rtcp_packets_;
    } else {
      ++number_of_rtp_packets_;
    }
    return true;
  }

  int number_of_rtp_packets() const { return number_of_rtp_packets_; }

  int number_of_rtcp_packets() const { return number_of_rtcp_packets_; }

 private:
  int number_of_rtp_packets_;
  int number_of_rtcp_packets_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketSender);
};

class PeerVideoSender : public VideoSender {
 public:
  PeerVideoSender(
      scoped_refptr<CastEnvironment> cast_environment,
      const VideoSenderConfig& video_config,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
      const CastInitializationCallback& cast_initialization_cb,
      transport::CastTransportSender* const transport_sender)
      : VideoSender(cast_environment,
                    video_config,
                    create_vea_cb,
                    create_video_encode_mem_cb,
                    cast_initialization_cb,
                    transport_sender) {}
  using VideoSender::OnReceivedCastFeedback;
};
}  // namespace

class VideoSenderTest : public ::testing::Test {
 protected:
  VideoSenderTest() {
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);
    transport::CastTransportVideoConfig transport_config;
    net::IPEndPoint dummy_endpoint;
    transport_sender_.reset(new transport::CastTransportSenderImpl(
        NULL,
        testing_clock_,
        dummy_endpoint,
        base::Bind(&UpdateCastTransportStatus),
        transport::BulkRawEventsCallback(),
        base::TimeDelta(),
        task_runner_,
        &transport_));
    transport_sender_->InitializeVideo(transport_config);
  }

  virtual ~VideoSenderTest() {}

  virtual void TearDown() OVERRIDE {
    video_sender_.reset();
    task_runner_->RunTasks();
  }

  static void UpdateCastTransportStatus(transport::CastTransportStatus status) {
    EXPECT_EQ(status, transport::TRANSPORT_VIDEO_INITIALIZED);
  }

  void InitEncoder(bool external) {
    VideoSenderConfig video_config;
    video_config.sender_ssrc = 1;
    video_config.incoming_feedback_ssrc = 2;
    video_config.rtcp_c_name = "video_test@10.1.1.1";
    video_config.rtp_config.payload_type = 127;
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
    video_config.codec = transport::kVp8;

    if (external) {
      scoped_ptr<VideoEncodeAccelerator> fake_vea(
          new test::FakeVideoEncodeAccelerator(task_runner_));
      video_sender_.reset(
          new PeerVideoSender(cast_environment_,
                              video_config,
                              base::Bind(&CreateVideoEncodeAccelerator,
                                         task_runner_,
                                         base::Passed(&fake_vea)),
                              base::Bind(&CreateSharedMemory),
                              base::Bind(&VideoSenderTest::InitializationResult,
                                         base::Unretained(this)),
                              transport_sender_.get()));
    } else {
      video_sender_.reset(
          new PeerVideoSender(cast_environment_,
                              video_config,
                              CreateDefaultVideoEncodeAcceleratorCallback(),
                              CreateDefaultVideoEncodeMemoryCallback(),
                              base::Bind(&VideoSenderTest::InitializationResult,
                                         base::Unretained(this)),
                              transport_sender_.get()));
    }
  }

  scoped_refptr<media::VideoFrame> GetNewVideoFrame() {
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(video_frame, kPixelValue);
    return video_frame;
  }

  void RunTasks(int during_ms) {
    for (int i = 0; i < during_ms; ++i) {
      // Call process the timers every 1 ms.
      testing_clock_->Advance(base::TimeDelta::FromMilliseconds(1));
      task_runner_->RunTasks();
    }
  }

  void InitializationResult(CastInitializationStatus result) {
    EXPECT_EQ(result, STATUS_VIDEO_INITIALIZED);
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  TestPacketSender transport_;
  scoped_ptr<transport::CastTransportSenderImpl> transport_sender_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<PeerVideoSender> video_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(VideoSenderTest);
};

TEST_F(VideoSenderTest, BuiltInEncoder) {
  InitEncoder(false);
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  task_runner_->RunTasks();
  EXPECT_GE(
      transport_.number_of_rtp_packets() + transport_.number_of_rtcp_packets(),
      1);
}

TEST_F(VideoSenderTest, ExternalEncoder) {
  InitEncoder(true);
  task_runner_->RunTasks();

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  task_runner_->RunTasks();

  // We need to run the task to cleanup the GPU instance.
  video_sender_.reset(NULL);
  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, RtcpTimer) {
  InitEncoder(false);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);

  RunTasks(max_rtcp_timeout.InMilliseconds());
  EXPECT_GE(transport_.number_of_rtp_packets(), 1);
  // Don't send RTCP prior to receiving an ACK.
  EXPECT_GE(transport_.number_of_rtcp_packets(), 0);
  // Build Cast msg and expect RTCP packet.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);
  RunTasks(max_rtcp_timeout.InMilliseconds());
  EXPECT_GE(transport_.number_of_rtcp_packets(), 1);
}

TEST_F(VideoSenderTest, ResendTimer) {
  InitEncoder(false);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  // ACK the key frame.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  base::TimeDelta max_resend_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtpMaxDelayMs);

  // Make sure that we do a re-send.
  RunTasks(max_resend_timeout.InMilliseconds());
  // Should have sent at least 3 packets.
  EXPECT_GE(
      transport_.number_of_rtp_packets() + transport_.number_of_rtcp_packets(),
      3);
}

TEST_F(VideoSenderTest, LogAckReceivedEvent) {
  InitEncoder(false);
  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  int num_frames = 10;
  for (int i = 0; i < num_frames; i++) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

    base::TimeTicks capture_time;
    video_sender_->InsertRawVideoFrame(video_frame, capture_time);
    RunTasks(33);
  }

  task_runner_->RunTasks();

  RtcpCastMessage cast_feedback(1);
  cast_feedback.ack_frame_id_ = num_frames - 1;

  video_sender_->OnReceivedCastFeedback(cast_feedback);

  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);

  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(kVideoAckReceived, frame_events.rbegin()->type);
  EXPECT_EQ(num_frames - 1u, frame_events.rbegin()->frame_id);

  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(VideoSenderTest, StopSendingIntheAbsenceOfAck) {
  InitEncoder(false);
  // Send a stream of frames and don't ACK; by default we shouldn't have more
  // than 4 frames in flight.
  // Store size in packets of frame 0, as it should be resent sue to timeout.
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  base::TimeTicks capture_time;
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);
  RunTasks(33);
  const int size_of_frame0 = transport_.number_of_rtp_packets();

  for (int i = 1; i < 4; ++i) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
    base::TimeTicks capture_time;
    video_sender_->InsertRawVideoFrame(video_frame, capture_time);
    RunTasks(33);
  }

  const int number_of_packets_sent = transport_.number_of_rtp_packets();
  // Send 4 more frames - they should not be sent to the transport, as we have
  // received any acks.
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
    base::TimeTicks capture_time;
    video_sender_->InsertRawVideoFrame(video_frame, capture_time);
    RunTasks(33);
  }

  EXPECT_EQ(number_of_packets_sent + size_of_frame0,
            transport_.number_of_rtp_packets());

  // Start acking and make sure we're back to steady-state.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);
  EXPECT_GE(
      transport_.number_of_rtp_packets() + transport_.number_of_rtcp_packets(),
      4);

  // Empty the pipeline.
  RunTasks(100);
  // Should have sent at least 7 packets.
  EXPECT_GE(
      transport_.number_of_rtp_packets() + transport_.number_of_rtcp_packets(),
      7);
}

}  // namespace cast
}  // namespace media
