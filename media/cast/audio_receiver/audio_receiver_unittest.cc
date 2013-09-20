// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/audio_receiver/audio_receiver.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_thread.h"
#include "media/cast/pacing/mock_paced_packet_sender.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

static const int kPacketSize = 1500;
static const int64 kStartMillisecond = 123456789;

class PeerAudioReceiver : public AudioReceiver {
 public:
  PeerAudioReceiver(scoped_refptr<CastThread> cast_thread,
                    const AudioReceiverConfig& audio_config,
                    PacedPacketSender* const packet_sender)
      : AudioReceiver(cast_thread, audio_config, packet_sender) {
  }
  using AudioReceiver::IncomingParsedRtpPacket;
};

class AudioReceiverTest : public ::testing::Test {
 protected:
  AudioReceiverTest() {
    // Configure the audio receiver to use PCM16.
    audio_config_.rtp_payload_type = 127;
    audio_config_.frequency = 16000;
    audio_config_.channels = 1;
    audio_config_.codec = kPcm16;
    audio_config_.use_external_decoder = false;
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeTaskRunner(&testing_clock_);
    cast_thread_ = new CastThread(task_runner_, task_runner_, task_runner_,
                                  task_runner_, task_runner_);
  }

  void Configure(bool use_external_decoder) {
    audio_config_.use_external_decoder = use_external_decoder;
    receiver_.reset(new
        PeerAudioReceiver(cast_thread_, audio_config_, &mock_transport_));
    receiver_->set_clock(&testing_clock_);
  }

  ~AudioReceiverTest() {}

  virtual void SetUp() {
  payload_.assign(kPacketSize, 0);
    // Always start with a key frame.
    rtp_header_.is_key_frame = true;
    rtp_header_.frame_id = 0;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
  }

  AudioReceiverConfig audio_config_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_ptr<PeerAudioReceiver> receiver_;
  scoped_refptr<CastThread> cast_thread_;
  base::SimpleTestTickClock testing_clock_;
};

TEST_F(AudioReceiverTest, GetOnePacketEncodedframe) {
  Configure(true);
  receiver_->IncomingParsedRtpPacket(
      payload_.data(), payload_.size(), rtp_header_);
  EncodedAudioFrame audio_frame;
  base::TimeTicks playout_time;
  EXPECT_TRUE(receiver_->GetEncodedAudioFrame(&audio_frame, &playout_time));
  EXPECT_EQ(0, audio_frame.frame_id);
  EXPECT_EQ(kPcm16, audio_frame.codec);
  task_runner_->RunTasks();
}

// TODO(mikhal): Add encoded frames.
TEST_F(AudioReceiverTest, GetRawFrame) {
}

}  // namespace cast
}  // namespace media
