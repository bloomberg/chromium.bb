// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/rtp_sender/rtp_sender.h"
#include "media/cast/rtcp/rtcp.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;

class LocalRtcpAudioSenderFeedback : public RtcpSenderFeedback {
 public:
  explicit LocalRtcpAudioSenderFeedback(AudioSender* audio_sender)
      : audio_sender_(audio_sender) {
  }

  virtual void OnReceivedCastFeedback(
      const RtcpCastMessage& cast_feedback) OVERRIDE {
    if (!cast_feedback.missing_frames_and_packets_.empty()) {
      audio_sender_->ResendPackets(cast_feedback.missing_frames_and_packets_);
    }
    VLOG(1) << "Received audio ACK "
            << static_cast<int>(cast_feedback.ack_frame_id_);
  }

 private:
  AudioSender* audio_sender_;
};

class LocalRtpSenderStatistics : public RtpSenderStatistics {
 public:
  explicit LocalRtpSenderStatistics(RtpSender* rtp_sender)
     : rtp_sender_(rtp_sender) {
  }

  virtual void GetStatistics(const base::TimeTicks& now,
                             RtcpSenderInfo* sender_info) OVERRIDE {
    rtp_sender_->RtpStatistics(now, sender_info);
  }

 private:
  RtpSender* rtp_sender_;
};

AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const AudioSenderConfig& audio_config,
                         PacedPacketSender* const paced_packet_sender)
      : cast_environment_(cast_environment),
        rtp_sender_(cast_environment, &audio_config, NULL,
                    paced_packet_sender),
        rtcp_feedback_(new LocalRtcpAudioSenderFeedback(this)),
        rtp_audio_sender_statistics_(
            new LocalRtpSenderStatistics(&rtp_sender_)),
        rtcp_(cast_environment,
              rtcp_feedback_.get(),
              paced_packet_sender,
              rtp_audio_sender_statistics_.get(),
              NULL,
              audio_config.rtcp_mode,
              base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
              audio_config.sender_ssrc,
              audio_config.incoming_feedback_ssrc,
              audio_config.rtcp_c_name),
        initialized_(false),
        weak_factory_(this) {
  if (audio_config.aes_iv_mask.size() == kAesKeySize &&
      audio_config.aes_key.size() == kAesKeySize) {
    iv_mask_ = audio_config.aes_iv_mask;
    crypto::SymmetricKey* key = crypto::SymmetricKey::Import(
        crypto::SymmetricKey::AES, audio_config.aes_key);
    encryptor_.reset(new crypto::Encryptor());
    encryptor_->Init(key, crypto::Encryptor::CTR, std::string());
  } else if (audio_config.aes_iv_mask.size() != 0 ||
             audio_config.aes_key.size() != 0) {
    DCHECK(false) << "Invalid crypto configuration";
  }
  if (!audio_config.use_external_encoder) {
    audio_encoder_ = new AudioEncoder(
        cast_environment, audio_config,
        base::Bind(&AudioSender::SendEncodedAudioFrame,
                   weak_factory_.GetWeakPtr()));
  }
}

AudioSender::~AudioSender() {}

void AudioSender::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!initialized_) {
    initialized_ = true;
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
  cast_environment_->Logging()->InsertFrameEvent(kAudioFrameReceived,
        GetVideoRtpTimestamp(recorded_time), kFrameIdUnknown);
  audio_encoder_->InsertAudio(audio_bus, recorded_time, done_callback);
}

void AudioSender::InsertCodedAudioFrame(const EncodedAudioFrame* audio_frame,
                                        const base::TimeTicks& recorded_time,
                                        const base::Closure callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_encoder_.get() == NULL) << "Invalid internal state";

  cast_environment_->Logging()->InsertFrameEvent(kAudioFrameReceived,
      GetVideoRtpTimestamp(recorded_time), kFrameIdUnknown);

  if (encryptor_) {
    EncodedAudioFrame encrypted_frame;
    if (!EncryptAudioFrame(*audio_frame, &encrypted_frame)) {
      // Logging already done.
      return;
    }
    rtp_sender_.IncomingEncodedAudioFrame(&encrypted_frame, recorded_time);
  } else {
    rtp_sender_.IncomingEncodedAudioFrame(audio_frame, recorded_time);
  }
  callback.Run();
}

void AudioSender::SendEncodedAudioFrame(
    scoped_ptr<EncodedAudioFrame> audio_frame,
    const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  InitializeTimers();
  if (encryptor_) {
    EncodedAudioFrame encrypted_frame;
    if (!EncryptAudioFrame(*audio_frame.get(), &encrypted_frame)) {
      // Logging already done.
      return;
    }
    rtp_sender_.IncomingEncodedAudioFrame(&encrypted_frame, recorded_time);
  } else {
    rtp_sender_.IncomingEncodedAudioFrame(audio_frame.get(), recorded_time);
  }
}

bool AudioSender::EncryptAudioFrame(const EncodedAudioFrame& audio_frame,
                                    EncodedAudioFrame* encrypted_frame) {
  DCHECK(encryptor_) << "Invalid state";

  if (!encryptor_->SetCounter(GetAesNonce(audio_frame.frame_id, iv_mask_))) {
    NOTREACHED() << "Failed to set counter";
    return false;
  }
  if (!encryptor_->Encrypt(audio_frame.data, &encrypted_frame->data)) {
    NOTREACHED() << "Encrypt error";
    return false;
  }
  encrypted_frame->codec = audio_frame.codec;
  encrypted_frame->frame_id = audio_frame.frame_id;
  encrypted_frame->samples = audio_frame.samples;
  return true;
}

void AudioSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtp_sender_.ResendPackets(missing_frames_and_packets);
}

void AudioSender::IncomingRtcpPacket(const uint8* packet, size_t length,
                                     const base::Closure callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_.IncomingRtcpPacket(packet, length);
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
}

void AudioSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next =
      rtcp_.TimeToSendNextRtcpReport() - cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&AudioSender::SendRtcpReport, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void AudioSender::SendRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // We don't send audio logging messages since all captured audio frames will
  // be sent.
  rtcp_.SendRtcpFromRtpSender(NULL);
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
