// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/audio_sender/audio_sender.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/pacing/mock_paced_packet_sender.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = GG_INT64_C(12345678900000);

using testing::_;

static void RelaseFrame(const PcmAudioFrame* frame) {
  delete frame;
}

class AudioSenderTest : public ::testing::Test {
 protected:
  AudioSenderTest() {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual void SetUp() {
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_environment_ = new CastEnvironment(&testing_clock_, task_runner_,
        task_runner_, task_runner_, task_runner_, task_runner_);
    AudioSenderConfig audio_config;
    audio_config.codec = kOpus;
    audio_config.use_external_encoder = false;
    audio_config.frequency = 48000;
    audio_config.channels = 2;
    audio_config.bitrate = 64000;
    audio_config.rtp_payload_type = 127;

    audio_sender_.reset(
        new AudioSender(cast_environment_, audio_config, &mock_transport_));
  }

  virtual ~AudioSenderTest() {}

  base::SimpleTestTickClock testing_clock_;
  MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_ptr<AudioSender> audio_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  EXPECT_CALL(mock_transport_, SendPackets(_)).Times(1);

  PcmAudioFrame* audio_frame = new PcmAudioFrame();
  audio_frame->channels = 2;
  audio_frame->frequency = 48000;
  audio_frame->samples.insert(audio_frame->samples.begin(), 480 * 2 * 2, 123);

  base::TimeTicks recorded_time;
  audio_sender_->InsertRawAudioFrame(audio_frame, recorded_time,
      base::Bind(&RelaseFrame, audio_frame));

  task_runner_->RunTasks();
}

TEST_F(AudioSenderTest, RtcpTimer) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).Times(1);

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);
  testing_clock_.Advance(max_rtcp_timeout);
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media
