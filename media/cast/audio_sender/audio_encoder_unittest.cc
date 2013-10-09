// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = GG_INT64_C(12345678900000);

static void RelaseFrame(const PcmAudioFrame* frame) {
  delete frame;
}

static void FrameEncoded(scoped_ptr<EncodedAudioFrame> encoded_frame,
                         const base::TimeTicks& recorded_time) {
}

class AudioEncoderTest : public ::testing::Test {
 protected:
  AudioEncoderTest() {
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

    audio_encoder_ = new AudioEncoder(cast_environment_, audio_config);
  }

  virtual ~AudioEncoderTest() {}

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<AudioEncoder> audio_encoder_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

TEST_F(AudioEncoderTest, Encode20ms) {
  PcmAudioFrame* audio_frame = new PcmAudioFrame();
  audio_frame->channels = 2;
  audio_frame->frequency = 48000;
  audio_frame->samples.insert(audio_frame->samples.begin(), 480 * 2 * 2, 123);

  base::TimeTicks recorded_time;
  audio_encoder_->InsertRawAudioFrame(audio_frame, recorded_time,
      base::Bind(&FrameEncoded),
      base::Bind(&RelaseFrame, audio_frame));
  task_runner_->RunTasks();
}

}  // namespace cast
}  // namespace media
