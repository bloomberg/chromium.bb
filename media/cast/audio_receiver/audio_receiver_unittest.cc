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
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/pacing/mock_paced_packet_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = GG_INT64_C(12345678900000);

namespace {
class TestAudioEncoderCallback
    : public base::RefCountedThreadSafe<TestAudioEncoderCallback> {
 public:
  TestAudioEncoderCallback() : num_called_(0) {}

  void SetExpectedResult(uint8 expected_frame_id,
                         const base::TimeTicks& expected_playout_time) {
    expected_frame_id_ = expected_frame_id;
    expected_playout_time_ = expected_playout_time;
  }

  void DeliverEncodedAudioFrame(
      scoped_ptr<transport::EncodedAudioFrame> audio_frame,
      const base::TimeTicks& playout_time) {
    EXPECT_EQ(expected_frame_id_, audio_frame->frame_id);
    EXPECT_EQ(transport::kPcm16, audio_frame->codec);
    EXPECT_EQ(expected_playout_time_, playout_time);
    num_called_++;
  }

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestAudioEncoderCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestAudioEncoderCallback>;

  int num_called_;
  uint8 expected_frame_id_;
  base::TimeTicks expected_playout_time_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioEncoderCallback);
};
}  // namespace

class PeerAudioReceiver : public AudioReceiver {
 public:
  PeerAudioReceiver(scoped_refptr<CastEnvironment> cast_environment,
                    const AudioReceiverConfig& audio_config,
                    transport::PacedPacketSender* const packet_sender)
      : AudioReceiver(cast_environment, audio_config, packet_sender) {}

  using AudioReceiver::IncomingParsedRtpPacket;
};

class AudioReceiverTest : public ::testing::Test {
 protected:
  AudioReceiverTest() {
    // Configure the audio receiver to use PCM16.
    audio_config_.rtp_payload_type = 127;
    audio_config_.frequency = 16000;
    audio_config_.channels = 1;
    audio_config_.codec = transport::kPcm16;
    audio_config_.use_external_decoder = false;
    audio_config_.feedback_ssrc = 1234;
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);

    CastLoggingConfig logging_config(GetDefaultCastReceiverLoggingConfig());
    logging_config.enable_raw_data_collection = true;

    cast_environment_ = new CastEnvironment(
        scoped_ptr<base::TickClock>(testing_clock_).Pass(), task_runner_,
        task_runner_, task_runner_, task_runner_, task_runner_, task_runner_,
        logging_config);

    test_audio_encoder_callback_ = new TestAudioEncoderCallback();
  }

  void Configure(bool use_external_decoder) {
    audio_config_.use_external_decoder = use_external_decoder;
    receiver_.reset(new PeerAudioReceiver(cast_environment_, audio_config_,
                                          &mock_transport_));
  }

  virtual ~AudioReceiverTest() {}

  static void DummyDeletePacket(const uint8* packet) {};

  virtual void SetUp() {
    payload_.assign(kMaxIpPacketSize, 0);
    rtp_header_.is_key_frame = true;
    rtp_header_.frame_id = 0;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
    rtp_header_.webrtc.header.timestamp = 0;
  }

  AudioReceiverConfig audio_config_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  transport::MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<PeerAudioReceiver> receiver_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<TestAudioEncoderCallback> test_audio_encoder_callback_;
};

TEST_F(AudioReceiverTest, GetOnePacketEncodedframe) {
  Configure(true);
  EXPECT_CALL(mock_transport_, SendRtcpPacket(testing::_)).Times(1);

  receiver_->IncomingParsedRtpPacket(payload_.data(), payload_.size(),
                                     rtp_header_);
  transport::EncodedAudioFrame audio_frame;
  base::TimeTicks playout_time;
  test_audio_encoder_callback_->SetExpectedResult(0,
                                                  testing_clock_->NowTicks());

  AudioFrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestAudioEncoderCallback::DeliverEncodedAudioFrame,
                 test_audio_encoder_callback_.get());

  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, test_audio_encoder_callback_->number_times_called());
}

TEST_F(AudioReceiverTest, MultiplePendingGetCalls) {
  Configure(true);
  EXPECT_CALL(mock_transport_, SendRtcpPacket(testing::_))
      .WillRepeatedly(testing::Return(true));

  AudioFrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestAudioEncoderCallback::DeliverEncodedAudioFrame,
                 test_audio_encoder_callback_.get());

  receiver_->GetEncodedAudioFrame(frame_encoded_callback);

  receiver_->IncomingParsedRtpPacket(payload_.data(), payload_.size(),
                                     rtp_header_);

  transport::EncodedAudioFrame audio_frame;
  base::TimeTicks playout_time;
  test_audio_encoder_callback_->SetExpectedResult(0,
                                                  testing_clock_->NowTicks());

  task_runner_->RunTasks();
  EXPECT_EQ(1, test_audio_encoder_callback_->number_times_called());

  TestRtcpPacketBuilder rtcp_packet;

  uint32 ntp_high;
  uint32 ntp_low;
  ConvertTimeTicksToNtp(testing_clock_->NowTicks(), &ntp_high, &ntp_low);
  rtcp_packet.AddSrWithNtp(audio_config_.feedback_ssrc, ntp_high, ntp_low,
                           rtp_header_.webrtc.header.timestamp);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));

  receiver_->IncomingPacket(rtcp_packet.GetPacket().Pass());

  // Make sure that we are not continuous and that the RTP timestamp represent a
  // time in the future.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = 2;
  rtp_header_.is_reference = true;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.webrtc.header.timestamp = 960;
  test_audio_encoder_callback_->SetExpectedResult(
      2, testing_clock_->NowTicks() + base::TimeDelta::FromMilliseconds(100));

  receiver_->IncomingParsedRtpPacket(payload_.data(), payload_.size(),
                                     rtp_header_);
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();

  // Frame 2 should not come out at this point in time.
  EXPECT_EQ(1, test_audio_encoder_callback_->number_times_called());

  // Through on one more pending callback.
  receiver_->GetEncodedAudioFrame(frame_encoded_callback);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(100));

  task_runner_->RunTasks();
  EXPECT_EQ(2, test_audio_encoder_callback_->number_times_called());

  test_audio_encoder_callback_->SetExpectedResult(3,
                                                  testing_clock_->NowTicks());

  // Through on one more pending audio frame.
  rtp_header_.frame_id = 3;
  rtp_header_.is_reference = false;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.webrtc.header.timestamp = 1280;
  receiver_->IncomingParsedRtpPacket(payload_.data(), payload_.size(),
                                     rtp_header_);

  receiver_->GetEncodedAudioFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(3, test_audio_encoder_callback_->number_times_called());
}

// TODO(mikhal): Add encoded frames.
TEST_F(AudioReceiverTest, GetRawFrame) {}

}  // namespace cast
}  // namespace media
