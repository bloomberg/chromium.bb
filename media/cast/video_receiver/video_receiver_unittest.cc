// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/pacing/mock_paced_packet_sender.h"
#include "media/cast/video_receiver/video_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using ::testing::_;

namespace {

const int kPacketSize = 1500;
const int64 kStartMillisecond = INT64_C(12345678900000);
const uint32 kFirstFrameId = 1234;

class FakeVideoClient {
 public:
  FakeVideoClient() : num_called_(0) {}
  virtual ~FakeVideoClient() {}

  void SetNextExpectedResult(uint32 expected_frame_id,
                             const base::TimeTicks& expected_playout_time) {
    expected_frame_id_ = expected_frame_id;
    expected_playout_time_ = expected_playout_time;
  }

  void DeliverEncodedVideoFrame(
      scoped_ptr<transport::EncodedVideoFrame> video_frame,
      const base::TimeTicks& playout_time) {
    ASSERT_FALSE(!video_frame)
        << "If at shutdown: There were unsatisfied requests enqueued.";
    EXPECT_EQ(expected_frame_id_, video_frame->frame_id);
    EXPECT_EQ(transport::kVp8, video_frame->codec);
    EXPECT_EQ(expected_playout_time_, playout_time);
    ++num_called_;
  }

  int number_times_called() const { return num_called_; }

 private:
  int num_called_;
  uint32 expected_frame_id_;
  base::TimeTicks expected_playout_time_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoClient);
};
}  // namespace

class VideoReceiverTest : public ::testing::Test {
 protected:
  VideoReceiverTest() {
    // Configure to use vp8 software implementation.
    config_.rtp_max_delay_ms = 100;
    config_.use_external_decoder = false;
    // Note: Frame rate must divide 1000 without remainder so the test code
    // doesn't have to account for rounding errors.
    config_.max_frame_rate = 25;
    config_.codec = transport::kVp8;
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);

    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);

    receiver_.reset(new VideoReceiver(
        cast_environment_, config_, &mock_transport_));
  }

  virtual ~VideoReceiverTest() {}

  virtual void SetUp() {
    payload_.assign(kPacketSize, 0);

    // Always start with a key frame.
    rtp_header_.is_key_frame = true;
    rtp_header_.frame_id = kFirstFrameId;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
    rtp_header_.webrtc.header.timestamp = 0;
  }

  void FeedOneFrameIntoReceiver() {
    receiver_->OnReceivedPayloadData(
        payload_.data(), payload_.size(), rtp_header_);
  }

  VideoReceiverConfig config_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  transport::MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  FakeVideoClient fake_video_client_;

  // Important for the VideoReceiver to be declared last, since its dependencies
  // must remain alive until after its destruction.
  scoped_ptr<VideoReceiver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(VideoReceiverTest);
};

TEST_F(VideoReceiverTest, GetOnePacketEncodedFrame) {
  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  EXPECT_CALL(mock_transport_, SendRtcpPacket(_))
      .WillRepeatedly(testing::Return(true));

  // Enqueue a request for a video frame.
  receiver_->GetEncodedVideoFrame(
      base::Bind(&FakeVideoClient::DeliverEncodedVideoFrame,
                 base::Unretained(&fake_video_client_)));

  // The request should not be satisfied since no packets have been received.
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_video_client_.number_times_called());

  // Deliver one video frame to the receiver and expect to get one frame back.
  fake_video_client_.SetNextExpectedResult(kFirstFrameId,
                                           testing_clock_->NowTicks());
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);

  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(kVideoAckSent, frame_events.begin()->type);
  EXPECT_EQ(rtp_header_.frame_id, frame_events.begin()->frame_id);
  EXPECT_EQ(rtp_header_.webrtc.header.timestamp,
            frame_events.begin()->rtp_timestamp);

  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(VideoReceiverTest, MultiplePendingGetCalls) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_))
      .WillRepeatedly(testing::Return(true));

  // Enqueue a request for an video frame.
  const VideoFrameEncodedCallback frame_encoded_callback =
      base::Bind(&FakeVideoClient::DeliverEncodedVideoFrame,
                 base::Unretained(&fake_video_client_));
  receiver_->GetEncodedVideoFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_video_client_.number_times_called());

  // Receive one video frame and expect to see the first request satisfied.
  fake_video_client_.SetNextExpectedResult(kFirstFrameId,
                                           testing_clock_->NowTicks());
  const base::TimeTicks time_at_first_frame_feed = testing_clock_->NowTicks();
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  testing_clock_->Advance(
      base::TimeDelta::FromSeconds(1) / config_.max_frame_rate);

  // Enqueue a second request for an video frame, but it should not be
  // fulfilled yet.
  receiver_->GetEncodedVideoFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // Receive one video frame out-of-order: Make sure that we are not continuous
  // and that the RTP timestamp represents a time in the future.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = kFirstFrameId + 2;
  rtp_header_.is_reference = true;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.webrtc.header.timestamp +=
      config_.rtp_max_delay_ms * kVideoFrequency / 1000;
  fake_video_client_.SetNextExpectedResult(
      kFirstFrameId + 2,
      time_at_first_frame_feed +
          base::TimeDelta::FromMilliseconds(config_.rtp_max_delay_ms));
  FeedOneFrameIntoReceiver();

  // Frame 2 should not come out at this point in time.
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // Enqueue a third request for an video frame.
  receiver_->GetEncodedVideoFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // After |rtp_max_delay_ms| has elapsed, Frame 2 is emitted (to satisfy the
  // second request) because a decision was made to skip over the no-show Frame
  // 1.
  testing_clock_->Advance(
      base::TimeDelta::FromMilliseconds(config_.rtp_max_delay_ms));
  task_runner_->RunTasks();
  EXPECT_EQ(2, fake_video_client_.number_times_called());

  // Receive Frame 3 and expect it to fulfill the third request immediately.
  rtp_header_.frame_id = kFirstFrameId + 3;
  rtp_header_.is_reference = false;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.webrtc.header.timestamp +=
      kVideoFrequency / config_.max_frame_rate;
  fake_video_client_.SetNextExpectedResult(kFirstFrameId + 3,
                                           testing_clock_->NowTicks());
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_video_client_.number_times_called());

  // Move forward another |rtp_max_delay_ms| and run any pending tasks (there
  // should be none).  Expect no additional frames where emitted.
  testing_clock_->Advance(
      base::TimeDelta::FromMilliseconds(config_.rtp_max_delay_ms));
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_video_client_.number_times_called());
}

}  // namespace cast
}  // namespace media
