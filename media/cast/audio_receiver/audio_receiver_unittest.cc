// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "media/cast/transport/pacing/mock_paced_packet_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using ::testing::_;

namespace {

const int64 kStartMillisecond = INT64_C(12345678900000);
const uint32 kFirstFrameId = 1234;

class FakeAudioClient {
 public:
  FakeAudioClient() : num_called_(0) {}
  virtual ~FakeAudioClient() {}

  void SetNextExpectedResult(uint32 expected_frame_id,
                             const base::TimeTicks& expected_playout_time) {
    expected_frame_id_ = expected_frame_id;
    expected_playout_time_ = expected_playout_time;
  }

  void DeliverEncodedAudioFrame(
      scoped_ptr<transport::EncodedAudioFrame> audio_frame,
      const base::TimeTicks& playout_time) {
    ASSERT_FALSE(!audio_frame)
        << "If at shutdown: There were unsatisfied requests enqueued.";
    EXPECT_EQ(expected_frame_id_, audio_frame->frame_id);
    EXPECT_EQ(transport::kPcm16, audio_frame->codec);
    EXPECT_EQ(expected_playout_time_, playout_time);
    num_called_++;
  }

  int number_times_called() const { return num_called_; }

 private:
  int num_called_;
  uint32 expected_frame_id_;
  base::TimeTicks expected_playout_time_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioClient);
};

}  // namespace

class AudioReceiverTest : public ::testing::Test {
 protected:
  AudioReceiverTest() {
    // Configure the audio receiver to use PCM16.
    audio_config_.rtp_payload_type = 127;
    audio_config_.frequency = 16000;
    audio_config_.channels = 1;
    audio_config_.codec = transport::kPcm16;
    audio_config_.use_external_decoder = true;
    audio_config_.feedback_ssrc = 1234;
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
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
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
    rtp_header_.webrtc.header.timestamp = 0;
  }

  void FeedOneFrameIntoReceiver() {
    receiver_->OnReceivedPayloadData(
        payload_.data(), payload_.size(), rtp_header_);
  }

  AudioReceiverConfig audio_config_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  transport::MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  FakeAudioClient fake_audio_client_;

  // Important for the AudioReceiver to be declared last, since its dependencies
  // must remain alive until after its destruction.
  scoped_ptr<AudioReceiver> receiver_;
};

TEST_F(AudioReceiverTest, GetOnePacketEncodedFrame) {
  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).Times(1);

  // Enqueue a request for an audio frame.
  receiver_->GetEncodedAudioFrame(
      base::Bind(&FakeAudioClient::DeliverEncodedAudioFrame,
                 base::Unretained(&fake_audio_client_)));

  // The request should not be satisfied since no packets have been received.
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_audio_client_.number_times_called());

  // Deliver one audio frame to the receiver and expect to get one frame back.
  fake_audio_client_.SetNextExpectedResult(kFirstFrameId,
                                           testing_clock_->NowTicks());
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);

  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(kAudioAckSent, frame_events.begin()->type);
  EXPECT_EQ(rtp_header_.frame_id, frame_events.begin()->frame_id);
  EXPECT_EQ(rtp_header_.webrtc.header.timestamp,
            frame_events.begin()->rtp_timestamp);

  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(AudioReceiverTest, MultiplePendingGetCalls) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_))
      .WillRepeatedly(testing::Return(true));

  // Enqueue a request for an audio frame.
  const AudioFrameEncodedCallback frame_encoded_callback =
      base::Bind(&FakeAudioClient::DeliverEncodedAudioFrame,
                 base::Unretained(&fake_audio_client_));
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(0, fake_audio_client_.number_times_called());

  // Receive one audio frame and expect to see the first request satisfied.
  fake_audio_client_.SetNextExpectedResult(kFirstFrameId,
                                           testing_clock_->NowTicks());
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  TestRtcpPacketBuilder rtcp_packet;

  uint32 ntp_high;
  uint32 ntp_low;
  ConvertTimeTicksToNtp(testing_clock_->NowTicks(), &ntp_high, &ntp_low);
  rtcp_packet.AddSrWithNtp(audio_config_.feedback_ssrc, ntp_high, ntp_low,
                           rtp_header_.webrtc.header.timestamp);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));

  receiver_->IncomingPacket(rtcp_packet.GetPacket().Pass());

  // Enqueue a second request for an audio frame, but it should not be
  // fulfilled yet.
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  // Receive one audio frame out-of-order: Make sure that we are not continuous
  // and that the RTP timestamp represents a time in the future.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = kFirstFrameId + 2;
  rtp_header_.is_reference = true;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.webrtc.header.timestamp = 960;
  fake_audio_client_.SetNextExpectedResult(
      kFirstFrameId + 2,
      testing_clock_->NowTicks() + base::TimeDelta::FromMilliseconds(100));
  FeedOneFrameIntoReceiver();

  // Frame 2 should not come out at this point in time.
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  // Enqueue a third request for an audio frame.
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, fake_audio_client_.number_times_called());

  // After 100 ms has elapsed, Frame 2 is emitted (to satisfy the second
  // request) because a decision was made to skip over the no-show Frame 1.
  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(100));
  task_runner_->RunTasks();
  EXPECT_EQ(2, fake_audio_client_.number_times_called());

  // Receive Frame 3 and expect it to fulfill the third request immediately.
  rtp_header_.frame_id = kFirstFrameId + 3;
  rtp_header_.is_reference = false;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.webrtc.header.timestamp = 1280;
  fake_audio_client_.SetNextExpectedResult(kFirstFrameId + 3,
                                           testing_clock_->NowTicks());
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_audio_client_.number_times_called());

  // Move forward another 100 ms and run any pending tasks (there should be
  // none).  Expect no additional frames where emitted.
  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(100));
  task_runner_->RunTasks();
  EXPECT_EQ(3, fake_audio_client_.number_times_called());
}

}  // namespace cast
}  // namespace media
