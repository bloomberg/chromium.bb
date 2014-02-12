// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/pacing/mock_paced_packet_sender.h"
#include "media/cast/video_receiver/video_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

static const int kPacketSize = 1500;
static const int64 kStartMillisecond = GG_INT64_C(12345678900000);

namespace media {
namespace cast {

using testing::_;

namespace {
// Was thread counted thread safe.
class TestVideoReceiverCallback
    : public base::RefCountedThreadSafe<TestVideoReceiverCallback> {
 public:
  TestVideoReceiverCallback() : num_called_(0) {}

  // TODO(mikhal): Set and check expectations.
  void DecodeComplete(const scoped_refptr<media::VideoFrame>& video_frame,
                      const base::TimeTicks& render_time) {
    ++num_called_;
  }

  void FrameToDecode(scoped_ptr<transport::EncodedVideoFrame> video_frame,
                     const base::TimeTicks& render_time) {
    EXPECT_TRUE(video_frame->key_frame);
    EXPECT_EQ(transport::kVp8, video_frame->codec);
    ++num_called_;
  }

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestVideoReceiverCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestVideoReceiverCallback>;

  int num_called_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoReceiverCallback);
};
}  // namespace

class PeerVideoReceiver : public VideoReceiver {
 public:
  PeerVideoReceiver(scoped_refptr<CastEnvironment> cast_environment,
                    const VideoReceiverConfig& video_config,
                    transport::PacedPacketSender* const packet_sender)
      : VideoReceiver(cast_environment, video_config, packet_sender) {}
  using VideoReceiver::IncomingParsedRtpPacket;
};

class VideoReceiverTest : public ::testing::Test {
 protected:
  VideoReceiverTest() {
    // Configure to use vp8 software implementation.
    config_.codec = transport::kVp8;
    config_.use_external_decoder = false;
    testing_clock_ = new base::SimpleTestTickClock();
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_,
                            task_runner_,
                            task_runner_,
                            task_runner_,
                            GetDefaultCastReceiverLoggingConfig());
    receiver_.reset(
        new PeerVideoReceiver(cast_environment_, config_, &mock_transport_));
    testing_clock_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    video_receiver_callback_ = new TestVideoReceiverCallback();

    payload_.assign(kPacketSize, 0);

    // Always start with a key frame.
    rtp_header_.is_key_frame = true;
    rtp_header_.frame_id = 0;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
  }

  virtual ~VideoReceiverTest() {}

  transport::MockPacedPacketSender mock_transport_;
  VideoReceiverConfig config_;
  scoped_ptr<PeerVideoReceiver> receiver_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.

  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<TestVideoReceiverCallback> video_receiver_callback_;

  DISALLOW_COPY_AND_ASSIGN(VideoReceiverTest);
};

TEST_F(VideoReceiverTest, GetOnePacketEncodedframe) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_))
      .WillRepeatedly(testing::Return(true));
  receiver_->IncomingParsedRtpPacket(
      payload_.data(), payload_.size(), rtp_header_);

  VideoFrameEncodedCallback frame_to_decode_callback = base::Bind(
      &TestVideoReceiverCallback::FrameToDecode, video_receiver_callback_);

  receiver_->GetEncodedVideoFrame(frame_to_decode_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(video_receiver_callback_->number_times_called(), 1);
}

TEST_F(VideoReceiverTest, MultiplePackets) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_))
      .WillRepeatedly(testing::Return(true));
  rtp_header_.max_packet_id = 2;
  receiver_->IncomingParsedRtpPacket(
      payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  ++rtp_header_.webrtc.header.sequenceNumber;
  receiver_->IncomingParsedRtpPacket(
      payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  receiver_->IncomingParsedRtpPacket(
      payload_.data(), payload_.size(), rtp_header_);

  VideoFrameEncodedCallback frame_to_decode_callback = base::Bind(
      &TestVideoReceiverCallback::FrameToDecode, video_receiver_callback_);

  receiver_->GetEncodedVideoFrame(frame_to_decode_callback);

  task_runner_->RunTasks();
  EXPECT_EQ(video_receiver_callback_->number_times_called(), 1);
}

TEST_F(VideoReceiverTest, GetOnePacketRawframe) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_))
      .WillRepeatedly(testing::Return(true));
  receiver_->IncomingParsedRtpPacket(
      payload_.data(), payload_.size(), rtp_header_);
  // Decode error - requires legal input.
  VideoFrameDecodedCallback frame_decoded_callback = base::Bind(
      &TestVideoReceiverCallback::DecodeComplete, video_receiver_callback_);
  receiver_->GetRawVideoFrame(frame_decoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(video_receiver_callback_->number_times_called(), 0);
}

// TODO(pwestin): add encoded frames.

}  // namespace cast
}  // namespace media
