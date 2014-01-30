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
    VLOG(1) << "Received audio ACK "
            << static_cast<int>(cast_feedback.ack_frame_id_);
  }

 private:
  AudioSender* audio_sender_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalRtcpAudioSenderFeedback);
};

class LocalRtpSenderStatistics : public RtpSenderStatistics {
 public:
  explicit LocalRtpSenderStatistics(
      transport::CastTransportSender* const transport_sender)
      : transport_sender_(transport_sender) {}

  virtual void GetStatistics(const base::TimeTicks& now,
                             transport::RtcpSenderInfo* sender_info) OVERRIDE {
    transport_sender_->RtpAudioStatistics(now, sender_info);
  }

 private:
  transport::CastTransportSender* const transport_sender_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalRtpSenderStatistics);
};

// TODO(mikhal): Reduce heap allocation when not needed.
AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const AudioSenderConfig& audio_config,
                         transport::CastTransportSender* const transport_sender)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      rtcp_feedback_(new LocalRtcpAudioSenderFeedback(this)),
      rtp_audio_sender_statistics_(
          new LocalRtpSenderStatistics(transport_sender_)),
      rtcp_(cast_environment,
            rtcp_feedback_.get(),
            transport_sender_,
            NULL,  // paced sender.
            rtp_audio_sender_statistics_.get(),
            NULL,
            audio_config.rtcp_mode,
            base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
            audio_config.sender_ssrc,
            audio_config.incoming_feedback_ssrc,
            audio_config.rtcp_c_name),
      timers_initialized_(false),
      initialization_status_(STATUS_INITIALIZED),
      weak_factory_(this) {
  if (!audio_config.use_external_encoder) {
    audio_encoder_ =
        new AudioEncoder(cast_environment,
                         audio_config,
                         base::Bind(&AudioSender::SendEncodedAudioFrame,
                                    weak_factory_.GetWeakPtr()));
    initialization_status_ = audio_encoder_->InitializationResult();
  }
}

AudioSender::~AudioSender() {}

void AudioSender::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!timers_initialized_) {
    timers_initialized_ = true;
    ScheduleNextRtcpReport();
  }
}

void AudioSender::InsertAudio(const AudioBus* audio_bus,
                              const base::TimeTicks& recorded_time,
                              const base::Closure& done_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_encoder_.get()) << "Invalid internal state";
  // TODO(mikhal): Resolve calculation of the audio rtp_timestamp for logging.
  // This is a tmp solution to allow the code to build.
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->Logging()->InsertFrameEvent(
      now,
      kAudioFrameReceived,
      GetVideoRtpTimestamp(recorded_time),
      kFrameIdUnknown);
  audio_encoder_->InsertAudio(audio_bus, recorded_time, done_callback);
}

void AudioSender::SendEncodedAudioFrame(
    scoped_ptr<transport::EncodedAudioFrame> audio_frame,
    const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  InitializeTimers();
  cast_environment_->PostTask(
      CastEnvironment::TRANSPORT,
      FROM_HERE,
      base::Bind(&AudioSender::SendEncodedAudioFrameToTransport,
                 base::Unretained(this),
                 base::Passed(&audio_frame),
                 recorded_time));
}

void AudioSender::SendEncodedAudioFrameToTransport(
    scoped_ptr<transport::EncodedAudioFrame> audio_frame,
    const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::TRANSPORT));
  transport_sender_->InsertCodedAudioFrame(audio_frame.get(), recorded_time);
}

void AudioSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->PostTask(
      CastEnvironment::TRANSPORT,
      FROM_HERE,
      base::Bind(&AudioSender::ResendPacketsOnTransportThread,
                 base::Unretained(this),
                 missing_frames_and_packets));
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
  // We don't send audio logging messages since all captured audio frames will
  // be sent.
  transport::RtcpSenderLogMessage empty_msg;
  rtcp_.SendRtcpFromRtpSender(empty_msg);
  ScheduleNextRtcpReport();
}

void AudioSender::ResendPacketsOnTransportThread(
    const transport::MissingFramesAndPacketsMap& missing_packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::TRANSPORT));
  transport_sender_->ResendPackets(true, missing_packets);
}

}  // namespace cast
}  // namespace media
