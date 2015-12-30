// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "media/base/media.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/constants.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_sender_impl.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  DVLOG(1) << "OperationalStatus transitioning from " << *out_status << " to "
           << in_status;
  *out_status = in_status;
}

}  // namespace

class TestPacketSender : public PacketSender {
 public:
  TestPacketSender() : number_of_rtp_packets_(0), number_of_rtcp_packets_(0) {}

  bool SendPacket(PacketRef packet, const base::Closure& cb) final {
    if (IsRtcpPacket(&packet->data[0], packet->data.size())) {
      ++number_of_rtcp_packets_;
    } else {
      // Check that at least one RTCP packet was sent before the first RTP
      // packet.  This confirms that the receiver will have the necessary lip
      // sync info before it has to calculate the playout time of the first
      // frame.
      if (number_of_rtp_packets_ == 0)
        EXPECT_LE(1, number_of_rtcp_packets_);
      ++number_of_rtp_packets_;
    }
    return true;
  }

  int64_t GetBytesSent() final { return 0; }

  int number_of_rtp_packets() const { return number_of_rtp_packets_; }

  int number_of_rtcp_packets() const { return number_of_rtcp_packets_; }

 private:
  int number_of_rtp_packets_;
  int number_of_rtcp_packets_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketSender);
};

class AudioSenderTest : public ::testing::Test {
 protected:
  AudioSenderTest() {
    InitializeMediaLibrary();
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_),
                            task_runner_, task_runner_, task_runner_);
    audio_config_.codec = CODEC_AUDIO_OPUS;
    audio_config_.use_external_encoder = false;
    audio_config_.frequency = kDefaultAudioSamplingRate;
    audio_config_.channels = 2;
    audio_config_.bitrate = kDefaultAudioEncoderBitrate;
    audio_config_.rtp_payload_type = 127;

    net::IPEndPoint dummy_endpoint;

    transport_sender_.reset(new CastTransportSenderImpl(
        NULL,
        testing_clock_,
        net::IPEndPoint(),
        dummy_endpoint,
        make_scoped_ptr(new base::DictionaryValue),
        base::Bind(&UpdateCastTransportStatus),
        BulkRawEventsCallback(),
        base::TimeDelta(),
        task_runner_,
        PacketReceiverCallback(),
        &transport_));
    OperationalStatus operational_status = STATUS_UNINITIALIZED;
    audio_sender_.reset(new AudioSender(
        cast_environment_,
        audio_config_,
        base::Bind(&SaveOperationalStatus, &operational_status),
        transport_sender_.get()));
    task_runner_->RunTasks();
    CHECK_EQ(STATUS_INITIALIZED, operational_status);
  }

  ~AudioSenderTest() override {}

  static void UpdateCastTransportStatus(CastTransportStatus status) {
    EXPECT_EQ(TRANSPORT_AUDIO_INITIALIZED, status);
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  TestPacketSender transport_;
  scoped_ptr<CastTransportSenderImpl> transport_sender_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<AudioSender> audio_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;
  AudioSenderConfig audio_config_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(20);
  scoped_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels,
                          audio_config_.frequency,
                          TestAudioBusFactory::kMiddleANoteFreq,
                          0.5f).NextAudioBus(kDuration));

  audio_sender_->InsertAudio(std::move(bus), testing_clock_->NowTicks());
  task_runner_->RunTasks();
  EXPECT_LE(1, transport_.number_of_rtp_packets());
  EXPECT_LE(1, transport_.number_of_rtcp_packets());
}

TEST_F(AudioSenderTest, RtcpTimer) {
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(20);
  scoped_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels,
                          audio_config_.frequency,
                          TestAudioBusFactory::kMiddleANoteFreq,
                          0.5f).NextAudioBus(kDuration));

  audio_sender_->InsertAudio(std::move(bus), testing_clock_->NowTicks());
  task_runner_->RunTasks();

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kRtcpReportIntervalMs * 3 / 2);
  testing_clock_->Advance(max_rtcp_timeout);
  task_runner_->RunTasks();
  EXPECT_LE(1, transport_.number_of_rtp_packets());
  EXPECT_LE(1, transport_.number_of_rtcp_packets());
}

}  // namespace cast
}  // namespace media
