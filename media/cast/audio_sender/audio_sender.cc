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
  LocalRtpSenderStatistics(
      transport::CastTransportSender* const transport_sender,
      int frequency)
      : transport_sender_(transport_sender),
        frequency_(0),
        sender_info_(),
        rtp_timestamp_(0) {
    transport_sender_->SubscribeAudioRtpStatsCallback(base::Bind(
        &LocalRtpSenderStatistics::StoreStatistics, base::Unretained(this)));
  }

  virtual void GetStatistics(const base::TimeTicks& now,
                             transport::RtcpSenderInfo* sender_info) OVERRIDE {
    // Update and return last stored statistics.
    uint32 ntp_seconds = 0;
    uint32 ntp_fraction = 0;
    uint32 rtp_timestamp = 0;
    if (rtp_timestamp_ > 0) {
      base::TimeDelta time_since_last_send = now - time_sent_;
      rtp_timestamp = rtp_timestamp_ + time_since_last_send.InMilliseconds() *
                                           (frequency_ / 1000);
      // Update NTP time to current time.
      ConvertTimeTicksToNtp(now, &ntp_seconds, &ntp_fraction);
    }
    // Populate sender info.
    sender_info->rtp_timestamp = rtp_timestamp;
    sender_info->ntp_seconds = sender_info_.ntp_seconds;
    sender_info->ntp_fraction = sender_info_.ntp_fraction;
    sender_info->send_packet_count = sender_info_.send_packet_count;
    sender_info->send_octet_count = sender_info_.send_octet_count;
  }

  void StoreStatistics(const transport::RtcpSenderInfo& sender_info,
                       base::TimeTicks time_sent,
                       uint32 rtp_timestamp) {
    sender_info_ = sender_info;
    time_sent_ = time_sent;
    rtp_timestamp_ = rtp_timestamp;
  }

 private:
  transport::CastTransportSender* const transport_sender_;
  int frequency_;
  transport::RtcpSenderInfo sender_info_;
  base::TimeTicks time_sent_;
  uint32 rtp_timestamp_;

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
          new LocalRtpSenderStatistics(transport_sender_,
                                       audio_config.frequency)),
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
