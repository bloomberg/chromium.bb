// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;

class LocalRtcpAudioSenderFeedback : public RtcpSenderFeedback {
 public:
  explicit LocalRtcpAudioSenderFeedback(AudioSender* audio_sender)
      : audio_sender_(audio_sender) {}

  virtual void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback)
      OVERRIDE {
    if (!cast_feedback.missing_frames_and_packets_.empty()) {
      audio_sender_->ResendPackets(cast_feedback.missing_frames_and_packets_);
    }
    VLOG(2) << "Received audio ACK "
            << static_cast<int>(cast_feedback.ack_frame_id_);
  }

 private:
  AudioSender* audio_sender_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalRtcpAudioSenderFeedback);
};

// TODO(mikhal): Reduce heap allocation when not needed.
AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const AudioSenderConfig& audio_config,
                         transport::CastTransportSender* const transport_sender)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      rtp_timestamp_helper_(audio_config.frequency),
      rtcp_feedback_(new LocalRtcpAudioSenderFeedback(this)),
      rtcp_(cast_environment,
            rtcp_feedback_.get(),
            transport_sender_,
            NULL,  // paced sender.
            NULL,
            audio_config.rtcp_mode,
            base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
            audio_config.rtp_config.ssrc,
            audio_config.incoming_feedback_ssrc,
            audio_config.rtcp_c_name,
            true),
      timers_initialized_(false),
      cast_initialization_cb_(STATUS_AUDIO_UNINITIALIZED),
      weak_factory_(this) {
  rtcp_.SetCastReceiverEventHistorySize(kReceiverRtcpEventHistorySize);
  if (!audio_config.use_external_encoder) {
    audio_encoder_.reset(
        new AudioEncoder(cast_environment,
                         audio_config,
                         base::Bind(&AudioSender::SendEncodedAudioFrame,
                                    weak_factory_.GetWeakPtr())));
    cast_initialization_cb_ = audio_encoder_->InitializationResult();
  }

  media::cast::transport::CastTransportAudioConfig transport_config;
  transport_config.codec = audio_config.codec;
  transport_config.rtp.config = audio_config.rtp_config;
  transport_config.frequency = audio_config.frequency;
  transport_config.channels = audio_config.channels;
  transport_config.rtp.max_outstanding_frames =
      audio_config.rtp_config.max_delay_ms / 100 + 1;
  transport_sender_->InitializeAudio(transport_config);
}

AudioSender::~AudioSender() {}

void AudioSender::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!timers_initialized_) {
    timers_initialized_ = true;
    ScheduleNextRtcpReport();
  }
}

void AudioSender::InsertAudio(scoped_ptr<AudioBus> audio_bus,
                              const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_encoder_.get()) << "Invalid internal state";
  audio_encoder_->InsertAudio(audio_bus.Pass(), recorded_time);
}

void AudioSender::SendEncodedAudioFrame(
    scoped_ptr<transport::EncodedAudioFrame> audio_frame,
    const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtp_timestamp_helper_.StoreLatestTime(recorded_time,
                                        audio_frame->rtp_timestamp);
  InitializeTimers();
  transport_sender_->InsertCodedAudioFrame(audio_frame.get(), recorded_time);
}

void AudioSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  transport_sender_->ResendPackets(true, missing_frames_and_packets);
}

void AudioSender::IncomingRtcpPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
}

void AudioSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next =
      rtcp_.TimeToSendNextRtcpReport() - cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&AudioSender::SendRtcpReport, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void AudioSender::SendRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  uint32 now_as_rtp_timestamp = 0;
  if (rtp_timestamp_helper_.GetCurrentTimeAsRtpTimestamp(
          now, &now_as_rtp_timestamp)) {
    rtcp_.SendRtcpFromRtpSender(now, now_as_rtp_timestamp);
  }
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
