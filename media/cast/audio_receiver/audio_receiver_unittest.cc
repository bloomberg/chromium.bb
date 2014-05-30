// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/audio_receiver/audio_receiver.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/transport/pacing/mock_paced_packet_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace media {
namespace cast {

namespace {

const uint32 kFirstFrameId = 1234;
const int kPlayoutDelayMillis = 300;

class FakeAudioClient {
 public:
  FakeAudioClient() : num_called_(0) {}
  virtual ~FakeAudioClient() {}

  void AddExpectedResult(uint32 expected_frame_id,
                         const base::TimeTicks& expected_playout_time) {
    expected_results_.push_back(
        std::make_pair(expected_frame_id, expected_playout_time));
  }

  void DeliverEncodedAudioFrame(
      scoped_ptr<transport::EncodedFrame> audio_frame) {
    SCOPED_TRACE(::testing::Message() << "num_called_ is " << num_called_);
    ASSERT_FALSE(!audio_frame)
        << "If at shutdown: There were unsatisfied requests enqueued.";
    ASSERT_FALSE(expected_results_.empty());
    EXPECT_EQ(expected_results_.front().first, audio_frame->frame_id);
    EXPECT_EQ(expected_results_.front().second, audio_frame->reference_time);
    expected_results_.pop_front();
    num_called_++;
  }

  int number_times_called() const { return num_called_; }

 private:
  std::deque<std::pair<uint32, base::TimeTicks> > expected_results_;
  int num_called_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioClient);
};

}  // namespace

class AudioReceiverTest : public ::testing::Test {
 protected:
  AudioReceiverTest() {
    // Configure the audio receiver to use PCM16.
    audio_config_ = GetDefaultAudioReceiverConfig();
    audio_config_.rtp_max_delay_ms = kPlayoutDelayMillis;
    audio_config_.frequency = 16000;
    audio_config_.channels = 1;
    audio_config_.codec.audio = transport::kPcm16;
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    start_time_ = testing_clock_->NowTicks();
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);

    cast_environment_ = new CastEnvironment(
        scoped_ptr<base::TickClock>(testing_clock_).Pass(),
        task_runner_,
        task_runner_,
        task_runner_);

    receiver_.reset(new AudioReceiver(cast_environment_, audio_config_,
                                      &mock_transport_));
  }

  virtual ~AudioReceiverTest() {}

  virtual void SetUp() {
    payload_.assign(kMaxIpPacketSize, 0);
    rtp_header_.is_key_frame = true;
    rtp_header_.frame_id = kFirstFrameId;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.reference_frame_id = rtp_header_.frame_id;
    rtp_header_.rtp_timestamp = 0;
  }

  void FeedOneFrameIntoReceiver() {
    receiver_->OnReceivedPayloadData(
        payload_.data(), payload_.size(), rtp_header_);
  }

  void FeedLipSyncInfoIntoReceiver() {
    const base::TimeTicks now = testing_clock_->NowTicks();
    const int64 rtp_timestamp = (now - start_time_) *
        audio_config_.frequency / base::TimeDelta::FromSeconds(1);
    CHECK_LE(0, rtp_timestamp);
    uint32 ntp_seconds;
    uint32 ntp_fraction;
    ConvertTimeTicksToNtp(now, &ntp_seconds, &ntp_fraction);
    TestRtcpPacketBuilder rtcp_packet;
    rtcp_packet.AddSrWithNtp(audio_config_.incoming_ssrc,
                             ntp_seconds, ntp_fraction,
                             static_cast<uint32>(rtp_timestamp));
    receiver_->IncomingPacket(rtcp_packet.GetPacket().Pass());
  }

  FrameReceiverConfig audio_config_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  base::TimeTicks start_time_;
  transport::MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  FakeAudioClient fake_audio_client_;

  // Important for the AudioReceiver to be declared last, since its dependencies
  // must remain alive until after its destruction.
  scoped_ptr<AudioReceiver> receiver_;
};

TEST_F(AudioReceiverTest, ReceivesOneFrame) {
  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  EXPECT_CALL(mock_transport_, SendRtcpPacket(_, _))
      .WillRepeatedly(testing::Return(true));

  FeedLipSyncInfoIntoReceiver();
  task_runner_->RunTasks();

  // Enqueue a request for an audio frame.
  receiver_->GetEncodedAudioFrame(
      base::Bind(&FakeAudioClient::DeliverEncodedAudioFrame,
                 base::Unretained(&fake_audio_client_)));

  // The request should not be satisfied since no packets have been received.
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_audio_client_.number_times_called());

  // Deliver one audio frame to the receiver and expect to get one frame back.
  const base::TimeDelta target_playout_delay =
      base::TimeDelta::FromMilliseconds(kPlayoutDelayMillis);
  fake_audio_client_.AddExpectedResult(
      kFirstFrameId, testing_clock_->NowTicks() + target_playout_delay);
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);

  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(FRAME_ACK_SENT, frame_events.begin()->type);
  EXPECT_EQ(AUDIO_EVENT, frame_events.begin()->media_type);
  EXPECT_EQ(rtp_header_.frame_id, frame_events.begin()->frame_id);
  EXPECT_EQ(rtp_header_.rtp_timestamp, frame_events.begin()->rtp_timestamp);

  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(AudioReceiverTest, ReceivesFramesSkippingWhenAppropriate) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_, _))
      .WillRepeatedly(testing::Return(true));

  const uint32 rtp_advance_per_frame =
      audio_config_.frequency / audio_config_.max_frame_rate;
  const base::TimeDelta time_advance_per_frame =
      base::TimeDelta::FromSeconds(1) / audio_config_.max_frame_rate;

  FeedLipSyncInfoIntoReceiver();
  task_runner_->RunTasks();
  const base::TimeTicks first_frame_capture_time = testing_clock_->NowTicks();

  // Enqueue a request for an audio frame.
  const FrameEncodedCallback frame_encoded_callback =
      base::Bind(&FakeAudioClient::DeliverEncodedAudioFrame,
                 base::Unretained(&fake_audio_client_));
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_audio_client_.number_times_called());

  // Receive one audio frame and expect to see the first request satisfied.
  const base::TimeDelta target_playout_delay =
      base::TimeDelta::FromMilliseconds(kPlayoutDelayMillis);
  fake_audio_client_.AddExpectedResult(
      kFirstFrameId, first_frame_capture_time + target_playout_delay);
  rtp_header_.rtp_timestamp = 0;
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  // Enqueue a second request for an audio frame, but it should not be
  // fulfilled yet.
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  // Receive one audio frame out-of-order: Make sure that we are not continuous
  // and that the RTP timestamp represents a time in the future.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = kFirstFrameId + 2;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.rtp_timestamp += 2 * rtp_advance_per_frame;
  fake_audio_client_.AddExpectedResult(
      kFirstFrameId + 2,
      first_frame_capture_time + 2 * time_advance_per_frame +
          target_playout_delay);
  FeedOneFrameIntoReceiver();

  // Frame 2 should not come out at this point in time.
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  // Enqueue a third request for an audio frame.
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  // Now, advance time forward such that the receiver is convinced it should
  // skip Frame 2.  Frame 3 is emitted (to satisfy the second request) because a
  // decision was made to skip over the no-show Frame 2.
  testing_clock_->Advance(2 * time_advance_per_frame + target_playout_delay);
  task_runner_->RunTasks();
  EXPECT_EQ(2, fake_audio_client_.number_times_called());

  // Receive Frame 4 and expect it to fulfill the third request immediately.
  rtp_header_.frame_id = kFirstFrameId + 3;
  rtp_header_.reference_frame_id = rtp_header_.frame_id - 1;
  rtp_header_.rtp_timestamp += rtp_advance_per_frame;
  fake_audio_client_.AddExpectedResult(
      kFirstFrameId + 3, first_frame_capture_time + 3 * time_advance_per_frame +
          target_playout_delay);
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_audio_client_.number_times_called());

  // Move forward to the playout time of an unreceived Frame 5.  Expect no
  // additional frames were emitted.
  testing_clock_->Advance(3 * time_advance_per_frame);
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_audio_client_.number_times_called());
}

}  // namespace cast
}  // namespace media
