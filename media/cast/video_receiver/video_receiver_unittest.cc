// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_thread.h"
#include "media/cast/pacing/mock_paced_packet_sender.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/video_receiver/video_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

static const int kPacketSize = 1500;
static const int64 kStartMillisecond = 123456789;

namespace media {
namespace cast {

using testing::_;

// was thread counted thread safe.
class TestVideoReceiverCallback :
    public base::RefCountedThreadSafe<TestVideoReceiverCallback> {
 public:
  TestVideoReceiverCallback()
      :num_called_(0) {}
  // TODO(mikhal): Set and check expectations.
  void DecodeComplete(scoped_ptr<I420VideoFrame> frame,
      const base::TimeTicks render_time) {
    ++num_called_;
  }
  int number_times_called() { return num_called_;}
 private:
  int num_called_;
};

class PeerVideoReceiver : public VideoReceiver {
 public:
  PeerVideoReceiver(scoped_refptr<CastThread> cast_thread,
                    const VideoReceiverConfig& video_config,
                    PacedPacketSender* const packet_sender)
      : VideoReceiver(cast_thread, video_config, packet_sender) {
  }
  using VideoReceiver::IncomingRtpPacket;
};


class VideoReceiverTest : public ::testing::Test {
 protected:
  VideoReceiverTest() {
    // Configure to use vp8 software implementation.
    config_.codec = kVp8;
    config_.use_external_decoder = false;
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_thread_ = new CastThread(task_runner_, task_runner_, task_runner_,
                                  task_runner_, task_runner_);
    receiver_.reset(new
        PeerVideoReceiver(cast_thread_, config_, &mock_transport_));
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    video_receiver_callback_ = new TestVideoReceiverCallback();
    receiver_->set_clock(&testing_clock_);
  }

  ~VideoReceiverTest() {}

  virtual void SetUp() {
    payload_.assign(kPacketSize, 0);

    // Always start with a key frame.
    rtp_header_.is_key_frame = true;
    rtp_header_.frame_id = 0;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
  }

  MockPacedPacketSender mock_transport_;
  VideoReceiverConfig config_;
  scoped_ptr<PeerVideoReceiver> receiver_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock testing_clock_;

  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastThread> cast_thread_;
  scoped_refptr<TestVideoReceiverCallback> video_receiver_callback_;
};

TEST_F(VideoReceiverTest, GetOnePacketEncodedframe) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).WillRepeatedly(
      testing::Return(true));
  receiver_->IncomingRtpPacket(payload_.data(), payload_.size(), rtp_header_);
  EncodedVideoFrame video_frame;
  base::TimeTicks render_time;
  EXPECT_TRUE(receiver_->GetEncodedVideoFrame(&video_frame, &render_time));
  EXPECT_TRUE(video_frame.key_frame);
  EXPECT_EQ(kVp8, video_frame.codec);
  task_runner_->RunTasks();
}

TEST_F(VideoReceiverTest, MultiplePackets) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).WillRepeatedly(
      testing::Return(true));
  rtp_header_.max_packet_id = 2;
  receiver_->IncomingRtpPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  ++rtp_header_.webrtc.header.sequenceNumber;
  receiver_->IncomingRtpPacket(payload_.data(), payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  receiver_->IncomingRtpPacket(payload_.data(), payload_.size(), rtp_header_);
  EncodedVideoFrame video_frame;
  base::TimeTicks render_time;
  EXPECT_TRUE(receiver_->GetEncodedVideoFrame(&video_frame, &render_time));
  task_runner_->RunTasks();
}

// TODO(pwestin): add encoded frames.
TEST_F(VideoReceiverTest, GetOnePacketRawframe) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).WillRepeatedly(
      testing::Return(true));
  receiver_->IncomingRtpPacket(payload_.data(), payload_.size(), rtp_header_);
  // Decode error - requires legal input.
  VideoFrameDecodedCallback frame_decoded_callback =
      base::Bind(&TestVideoReceiverCallback::DecodeComplete,
                 video_receiver_callback_);
  receiver_->GetRawVideoFrame(frame_decoded_callback);
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media


