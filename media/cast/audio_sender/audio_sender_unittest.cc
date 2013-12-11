// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/media.h"
#include "media/cast/audio_sender/audio_sender.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/pacing/mock_paced_packet_sender.h"
#include "media/cast/test/audio_utility.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = GG_INT64_C(12345678900000);

using testing::_;
using testing::AtLeast;

class AudioSenderTest : public ::testing::Test {
 protected:
  AudioSenderTest() {
    InitializeMediaLibraryForTesting();
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual void SetUp() {
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_environment_ = new CastEnvironment(&testing_clock_, task_runner_,
        task_runner_, task_runner_, task_runner_, task_runner_,
        GetDefaultCastLoggingConfig());
    audio_config_.codec = kOpus;
    audio_config_.use_external_encoder = false;
    audio_config_.frequency = kDefaultAudioSamplingRate;
    audio_config_.channels = 2;
    audio_config_.bitrate = kDefaultAudioEncoderBitrate;
    audio_config_.rtp_payload_type = 127;

    audio_sender_.reset(
        new AudioSender(cast_environment_, audio_config_, &mock_transport_));
  }

  virtual ~AudioSenderTest() {}

  base::SimpleTestTickClock testing_clock_;
  MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_ptr<AudioSender> audio_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;
  AudioSenderConfig audio_config_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  EXPECT_CALL(mock_transport_, SendPackets(_)).Times(AtLeast(1));

  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(20);
  scoped_ptr<AudioBus> bus(TestAudioBusFactory(
      audio_config_.channels, audio_config_.frequency,
      TestAudioBusFactory::kMiddleANoteFreq, 0.5f).NextAudioBus(kDuration));

  base::TimeTicks recorded_time = base::TimeTicks::Now();
  audio_sender_->InsertAudio(
      bus.get(), recorded_time,
      base::Bind(base::IgnoreResult(&scoped_ptr<AudioBus>::release),
                 base::Unretained(&bus)));
  task_runner_->RunTasks();

  EXPECT_TRUE(!bus) << "AudioBus wasn't released after use.";
}

TEST_F(AudioSenderTest, RtcpTimer) {
  EXPECT_CALL(mock_transport_, SendPackets(_)).Times(AtLeast(1));
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).Times(1);

  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(20);
  scoped_ptr<AudioBus> bus(TestAudioBusFactory(
      audio_config_.channels, audio_config_.frequency,
      TestAudioBusFactory::kMiddleANoteFreq, 0.5f).NextAudioBus(kDuration));

  base::TimeTicks recorded_time = base::TimeTicks::Now();
  audio_sender_->InsertAudio(
      bus.get(), recorded_time,
      base::Bind(base::IgnoreResult(&scoped_ptr<AudioBus>::release),
                 base::Unretained(&bus)));
  task_runner_->RunTasks();

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);
  testing_clock_.Advance(max_rtcp_timeout);
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media
