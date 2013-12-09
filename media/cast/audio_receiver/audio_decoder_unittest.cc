// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/audio_receiver/audio_decoder.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {
class TestRtpPayloadFeedback : public RtpPayloadFeedback {
 public:
  TestRtpPayloadFeedback() {}
  virtual ~TestRtpPayloadFeedback() {}

  virtual void CastFeedback(const RtcpCastMessage& cast_feedback) OVERRIDE {
    EXPECT_EQ(1u, cast_feedback.ack_frame_id_);
    EXPECT_EQ(0u, cast_feedback.missing_frames_and_packets_.size());
  }
};
}  // namespace.

class AudioDecoderTest : public ::testing::Test {
 protected:
  AudioDecoderTest() {
    testing_clock_.Advance(base::TimeDelta::FromMilliseconds(1234));
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_environment_ = new CastEnvironment(&testing_clock_, task_runner_,
        task_runner_, task_runner_, task_runner_, task_runner_,
        GetDefaultCastLoggingConfig());
  }
  virtual ~AudioDecoderTest() {}

  void Configure(const AudioReceiverConfig& audio_config) {
    audio_decoder_.reset(
        new AudioDecoder(cast_environment_, audio_config, &cast_feedback_));
  }

  TestRtpPayloadFeedback cast_feedback_;
  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<AudioDecoder> audio_decoder_;
};

TEST_F(AudioDecoderTest, Pcm16MonoNoResampleOnePacket) {
  AudioReceiverConfig audio_config;
  audio_config.rtp_payload_type = 127;
  audio_config.frequency = 16000;
  audio_config.channels = 1;
  audio_config.codec = kPcm16;
  audio_config.use_external_decoder = false;
  Configure(audio_config);

  RtpCastHeader rtp_header;
  rtp_header.webrtc.header.payloadType = 127;
  rtp_header.webrtc.header.sequenceNumber = 1234;
  rtp_header.webrtc.header.timestamp = 0x87654321;
  rtp_header.webrtc.header.ssrc = 0x12345678;
  rtp_header.webrtc.header.paddingLength = 0;
  rtp_header.webrtc.header.headerLength = 12;
  rtp_header.webrtc.type.Audio.channel = 1;
  rtp_header.webrtc.type.Audio.isCNG = false;

  std::vector<int16> payload(640, 0x1234);
  int number_of_10ms_blocks = 4;
  int desired_frequency = 16000;
  PcmAudioFrame audio_frame;
  uint32 rtp_timestamp;

  EXPECT_FALSE(audio_decoder_->GetRawAudioFrame(number_of_10ms_blocks,
                                                desired_frequency,
                                                &audio_frame,
                                                &rtp_timestamp));

  uint8* payload_data = reinterpret_cast<uint8*>(&payload[0]);
  size_t payload_size = payload.size() * sizeof(int16);

  audio_decoder_->IncomingParsedRtpPacket(payload_data,
      payload_size, rtp_header);

  EXPECT_TRUE(audio_decoder_->GetRawAudioFrame(number_of_10ms_blocks,
                                               desired_frequency,
                                               &audio_frame,
                                               &rtp_timestamp));
  EXPECT_EQ(1, audio_frame.channels);
  EXPECT_EQ(16000, audio_frame.frequency);
  EXPECT_EQ(640ul, audio_frame.samples.size());
  // First 10 samples per channel are 0 from NetEq.
  for (size_t i = 10; i < audio_frame.samples.size(); ++i) {
    EXPECT_EQ(0x3412, audio_frame.samples[i]);
  }
}

TEST_F(AudioDecoderTest, Pcm16StereoNoResampleTwoPackets) {
  AudioReceiverConfig audio_config;
  audio_config.rtp_payload_type = 127;
  audio_config.frequency = 16000;
  audio_config.channels = 2;
  audio_config.codec = kPcm16;
  audio_config.use_external_decoder = false;
  Configure(audio_config);

  RtpCastHeader rtp_header;
  rtp_header.frame_id = 0;
  rtp_header.webrtc.header.payloadType = 127;
  rtp_header.webrtc.header.sequenceNumber = 1234;
  rtp_header.webrtc.header.timestamp = 0x87654321;
  rtp_header.webrtc.header.ssrc = 0x12345678;
  rtp_header.webrtc.header.paddingLength = 0;
  rtp_header.webrtc.header.headerLength = 12;

  rtp_header.webrtc.type.Audio.isCNG = false;
  rtp_header.webrtc.type.Audio.channel = 2;

  std::vector<int16> payload(640, 0x1234);

  uint8* payload_data = reinterpret_cast<uint8*>(&payload[0]);
  size_t payload_size = payload.size() * sizeof(int16);

  audio_decoder_->IncomingParsedRtpPacket(payload_data,
      payload_size, rtp_header);

  int number_of_10ms_blocks = 2;
  int desired_frequency = 16000;
  PcmAudioFrame audio_frame;
  uint32 rtp_timestamp;

  EXPECT_TRUE(audio_decoder_->GetRawAudioFrame(number_of_10ms_blocks,
                                               desired_frequency,
                                               &audio_frame,
                                               &rtp_timestamp));
  EXPECT_EQ(2, audio_frame.channels);
  EXPECT_EQ(16000, audio_frame.frequency);
  EXPECT_EQ(640ul, audio_frame.samples.size());
  // First 10 samples per channel are 0 from NetEq.
  for (size_t i = 10 * audio_config.channels; i < audio_frame.samples.size();
      ++i) {
    EXPECT_EQ(0x3412, audio_frame.samples[i]);
  }

  rtp_header.frame_id++;
  rtp_header.webrtc.header.sequenceNumber++;
  rtp_header.webrtc.header.timestamp += (audio_config.frequency / 100) * 2 * 2;

  audio_decoder_->IncomingParsedRtpPacket(payload_data,
      payload_size, rtp_header);

  EXPECT_TRUE(audio_decoder_->GetRawAudioFrame(number_of_10ms_blocks,
                                               desired_frequency,
                                               &audio_frame,
                                               &rtp_timestamp));
  EXPECT_EQ(2, audio_frame.channels);
  EXPECT_EQ(16000, audio_frame.frequency);
  EXPECT_EQ(640ul, audio_frame.samples.size());
  for (size_t i = 0; i < audio_frame.samples.size(); ++i) {
    EXPECT_NEAR(0x3412, audio_frame.samples[i], 1000);
  }
  // Test cast callback.
  audio_decoder_->SendCastMessage();
  testing_clock_.Advance(base::TimeDelta::FromMilliseconds(33));
  audio_decoder_->SendCastMessage();
}

TEST_F(AudioDecoderTest, Pcm16Resample) {
  AudioReceiverConfig audio_config;
  audio_config.rtp_payload_type = 127;
  audio_config.frequency = 16000;
  audio_config.channels = 2;
  audio_config.codec = kPcm16;
  audio_config.use_external_decoder = false;
  Configure(audio_config);

  RtpCastHeader rtp_header;
  rtp_header.webrtc.header.payloadType = 127;
  rtp_header.webrtc.header.sequenceNumber = 1234;
  rtp_header.webrtc.header.timestamp = 0x87654321;
  rtp_header.webrtc.header.ssrc = 0x12345678;
  rtp_header.webrtc.header.paddingLength = 0;
  rtp_header.webrtc.header.headerLength = 12;

  rtp_header.webrtc.type.Audio.isCNG = false;
  rtp_header.webrtc.type.Audio.channel = 2;

  std::vector<int16> payload(640, 0x1234);

  uint8* payload_data = reinterpret_cast<uint8*>(&payload[0]);
  size_t payload_size = payload.size() * sizeof(int16);

  audio_decoder_->IncomingParsedRtpPacket(payload_data,
      payload_size, rtp_header);

  int number_of_10ms_blocks = 2;
  int desired_frequency = 48000;
  PcmAudioFrame audio_frame;
  uint32 rtp_timestamp;

  EXPECT_TRUE(audio_decoder_->GetRawAudioFrame(number_of_10ms_blocks,
                                               desired_frequency,
                                               &audio_frame,
                                               &rtp_timestamp));

  EXPECT_EQ(2, audio_frame.channels);
  EXPECT_EQ(48000, audio_frame.frequency);
  EXPECT_EQ(1920ul, audio_frame.samples.size());  // Upsampled to 48 KHz.
  int count = 0;
  // Resampling makes the variance worse.
  for (size_t i = 100 * audio_config.channels; i < audio_frame.samples.size();
      ++i) {
    EXPECT_NEAR(0x3412, audio_frame.samples[i], 400);
    if (0x3412 == audio_frame.samples[i])  count++;
  }
}

}  // namespace cast
}  // namespace media
