// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/pacing/mock_paced_packet_sender.h"
#include "media/cast/video_receiver/video_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace media {
namespace cast {

namespace {

const int kPacketSize = 1500;
const uint32 kFirstFrameId = 1234;
const int kPlayoutDelayMillis = 100;

class FakeVideoClient {
 public:
  FakeVideoClient() : num_called_(0) {}
  virtual ~FakeVideoClient() {}

  void AddExpectedResult(uint32 expected_frame_id,
                         const base::TimeTicks& expected_playout_time) {
    expected_results_.push_back(
        std::make_pair(expected_frame_id, expected_playout_time));
  }

  void DeliverEncodedVideoFrame(
      scoped_ptr<transport::EncodedFrame> video_frame) {
    SCOPED_TRACE(::testing::Message() << "num_called_ is " << num_called_);
    ASSERT_FALSE(!video_frame)
        << "If at shutdown: There were unsatisfied requests enqueued.";
    ASSERT_FALSE(expected_results_.empty());
    EXPECT_EQ(expected_results_.front().first, video_frame->frame_id);
    EXPECT_EQ(expected_results_.front().second, video_frame->reference_time);
    expected_results_.pop_front();
    ++num_called_;
  }

  int number_times_called() const { return num_called_; }

 private:
  std::deque<std::pair<uint32, base::TimeTicks> > expected_results_;
  int num_called_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoClient);
};
}  // namespace

class VideoReceiverTest : public ::testing::Test {
 protected:
  VideoReceiverTest() {
    config_.rtp_max_delay_ms = kPlayoutDelayMillis;
    config_.use_external_decoder = false;
    // Note: Frame rate must divide 1000 without remainder so the test code
    // doesn't have to account for rounding errors.
    config_.max_frame_rate = 25;
    config_.codec = transport::kVp8;  // Frame skipping not allowed.
    config_.feedback_ssrc = 1234;
    config_.incoming_ssrc = 5678;
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    start_time_ = testing_clock_->NowTicks();
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
    rtp_header_.reference_frame_id = rtp_header_.frame_id;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
  }

  void FeedOneFrameIntoReceiver() {
    receiver_->OnReceivedPayloadData(
        payload_.data(), payload_.size(), rtp_header_);
  }

  void FeedLipSyncInfoIntoReceiver() {
    const base::TimeTicks now = testing_clock_->NowTicks();
    const int64 rtp_timestamp = (now - start_time_) *
        kVideoFrequency / base::TimeDelta::FromSeconds(1);
    CHECK_LE(0, rtp_timestamp);
    uint32 ntp_seconds;
    uint32 ntp_fraction;
    ConvertTimeTicksToNtp(now, &ntp_seconds, &ntp_fraction);
    TestRtcpPacketBuilder rtcp_packet;
    rtcp_packet.AddSrWithNtp(config_.incoming_ssrc,
                             ntp_seconds, ntp_fraction,
                             static_cast<uint32>(rtp_timestamp));
    receiver_->IncomingPacket(rtcp_packet.GetPacket().Pass());
  }

  VideoReceiverConfig config_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  base::TimeTicks start_time_;
  transport::MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  FakeVideoClient fake_video_client_;

  // Important for the VideoReceiver to be declared last, since its dependencies
  // must remain alive until after its destruction.
  scoped_ptr<VideoReceiver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(VideoReceiverTest);
};

TEST_F(VideoReceiverTest, ReceivesOneFrame) {
  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  EXPECT_CALL(mock_transport_, SendRtcpPacket(_, _))
      .WillRepeatedly(testing::Return(true));

  FeedLipSyncInfoIntoReceiver();
  task_runner_->RunTasks();

  // Enqueue a request for a video frame.
  receiver_->GetEncodedVideoFrame(
      base::Bind(&FakeVideoClient::DeliverEncodedVideoFrame,
                 base::Unretained(&fake_video_client_)));

  // The request should not be satisfied since no packets have been received.
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_video_client_.number_times_called());

  // Deliver one video frame to the receiver and expect to get one frame back.
  const base::TimeDelta target_playout_delay =
      base::TimeDelta::FromMilliseconds(kPlayoutDelayMillis);
  fake_video_client_.AddExpectedResult(
      kFirstFrameId, testing_clock_->NowTicks() + target_playout_delay);
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);

  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(FRAME_ACK_SENT, frame_events.begin()->type);
  EXPECT_EQ(rtp_header_.frame_id, frame_events.begin()->frame_id);
  EXPECT_EQ(rtp_header_.rtp_timestamp, frame_events.begin()->rtp_timestamp);

  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(VideoReceiverTest, ReceivesFramesRefusingToSkipAny) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_, _))
      .WillRepeatedly(testing::Return(true));

  const uint32 rtp_advance_per_frame = kVideoFrequency / config_.max_frame_rate;
  const base::TimeDelta time_advance_per_frame =
      base::TimeDelta::FromSeconds(1) / config_.max_frame_rate;

  // Feed and process lip sync in receiver.
  FeedLipSyncInfoIntoReceiver();
  task_runner_->RunTasks();
  const base::TimeTicks first_frame_capture_time = testing_clock_->NowTicks();

  // Enqueue a request for a video frame.
  const FrameEncodedCallback frame_encoded_callback =
      base::Bind(&FakeVideoClient::DeliverEncodedVideoFrame,
                 base::Unretained(&fake_video_client_));
  receiver_->GetEncodedVideoFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_video_client_.number_times_called());

  // Receive one video frame and expect to see the first request satisfied.
  const base::TimeDelta target_playout_delay =
      base::TimeDelta::FromMilliseconds(kPlayoutDelayMillis);
  fake_video_client_.AddExpectedResult(
      kFirstFrameId, first_frame_capture_time + target_playout_delay);
  rtp_header_.rtp_timestamp = 0;
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // Enqueue a second request for a video frame, but it should not be
  // fulfilled yet.
  receiver_->GetEncodedVideoFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // Receive one video frame out-of-order: Make sure that we are not continuous
  // and that the RTP timestamp represents a time in the future.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = kFirstFrameId + 2;  // "Frame 3"
  rtp_header_.reference_frame_id = kFirstFrameId + 1;  // "Frame 2"
  rtp_header_.rtp_timestamp += 2 * rtp_advance_per_frame;
  FeedOneFrameIntoReceiver();

  // Frame 2 should not come out at this point in time.
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // Enqueue a third request for a video frame.
  receiver_->GetEncodedVideoFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // Now, advance time forward such that Frame 2 is now too late for playback.
  // Regardless, the receiver must NOT emit Frame 3 yet because it is not
  // allowed to skip frames for VP8.
  testing_clock_->Advance(2 * time_advance_per_frame + target_playout_delay);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_video_client_.number_times_called());

  // Now receive Frame 2 and expect both the second and third requests to be
  // fulfilled immediately.
  fake_video_client_.AddExpectedResult(
      kFirstFrameId + 1,  // "Frame 2"
      first_frame_capture_time + 1 * time_advance_per_frame +
          target_playout_delay);
  fake_video_client_.AddExpectedResult(
      kFirstFrameId + 2,  // "Frame 3"
      first_frame_capture_time + 2 * time_advance_per_frame +
          target_playout_delay);
  --rtp_header_.frame_id;  // "Frame 2"
  --rtp_header_.reference_frame_id;  // "Frame 1"
  rtp_header_.rtp_timestamp -= rtp_advance_per_frame;
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_video_client_.number_times_called());

  // Move forward to the playout time of an unreceived Frame 5.  Expect no
  // additional frames were emitted.
  testing_clock_->Advance(3 * time_advance_per_frame);
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_video_client_.number_times_called());
}

}  // namespace cast
}  // namespace media
