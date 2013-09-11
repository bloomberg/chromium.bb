// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/platform_thread.h"
#include "media/cast/audio_sender/audio_sender.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_thread.h"
#include "media/cast/pacing/mock_paced_packet_sender.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = 123456789;

using base::RunLoop;
using testing::_;

static void RelaseFrame(const PcmAudioFrame* frame) {
  delete frame;
};

class AudioSenderTest : public ::testing::Test {
 protected:
  AudioSenderTest() {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual void SetUp() {
    cast_thread_ = new CastThread(MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current());
    AudioSenderConfig audio_config;
    audio_config.codec = kOpus;
    audio_config.use_external_encoder = false;
    audio_config.frequency = 48000;
    audio_config.channels = 2;
    audio_config.bitrate = 64000;
    audio_config.rtp_payload_type = 127;

    audio_sender_.reset(
        new AudioSender(cast_thread_, audio_config, &mock_transport_));
    audio_sender_->set_clock(&testing_clock_);
  }

  ~AudioSenderTest() {}

  base::MessageLoop loop_;
  MockPacedPacketSender mock_transport_;
  base::SimpleTestTickClock testing_clock_;
  scoped_ptr<AudioSender> audio_sender_;
  scoped_refptr<CastThread> cast_thread_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  EXPECT_CALL(mock_transport_, SendPacket(_, _)).Times(1);

  RunLoop run_loop;

  PcmAudioFrame* audio_frame = new PcmAudioFrame();
  audio_frame->channels = 2;
  audio_frame->frequency = 48000;
  audio_frame->samples.insert(audio_frame->samples.begin(), 480 * 2 * 2, 123);

  base::TimeTicks recorded_time;
  audio_sender_->InsertRawAudioFrame(audio_frame, recorded_time,
      base::Bind(&RelaseFrame, audio_frame));
  run_loop.RunUntilIdle();
}

TEST_F(AudioSenderTest, RtcpTimer) {
  EXPECT_CALL(mock_transport_, SendRtcpPacket(_)).Times(1);

  RunLoop run_loop;
  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);
  testing_clock_.Advance(max_rtcp_timeout);

  // TODO(pwestin): haven't found a way to make the post delayed task to go
  // faster than a real-time.
  base::PlatformThread::Sleep(max_rtcp_timeout);
  run_loop.RunUntilIdle();
}

}  // namespace cast
}  // namespace media
