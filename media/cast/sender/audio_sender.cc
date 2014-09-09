// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/sender/audio_encoder.h"

namespace media {
namespace cast {
namespace {

// TODO(miu): This should be specified in AudioSenderConfig, but currently it is
// fixed to 100 FPS (i.e., 10 ms per frame), and AudioEncoder assumes this as
// well.
const int kAudioFrameRate = 100;

}  // namespace

AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const AudioSenderConfig& audio_config,
                         CastTransportSender* const transport_sender)
    : FrameSender(
        cast_environment,
        true,
        transport_sender,
        base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
        audio_config.frequency,
        audio_config.ssrc,
        kAudioFrameRate * 2.0, // We lie to increase max outstanding frames.
        audio_config.target_playout_delay,
        NewFixedCongestionControl(audio_config.bitrate)),
      samples_in_encoder_(0),
      weak_factory_(this) {
  cast_initialization_status_ = STATUS_AUDIO_UNINITIALIZED;
  VLOG(1) << "max_unacked_frames " << max_unacked_frames_;
  DCHECK_GT(max_unacked_frames_, 0);

  if (!audio_config.use_external_encoder) {
    audio_encoder_.reset(
        new AudioEncoder(cast_environment,
                         audio_config.channels,
                         audio_config.frequency,
                         audio_config.bitrate,
                         audio_config.codec,
                         base::Bind(&AudioSender::OnEncodedAudioFrame,
                                    weak_factory_.GetWeakPtr(),
                                    audio_config.bitrate)));
    cast_initialization_status_ = audio_encoder_->InitializationResult();
  } else {
    NOTREACHED();  // No support for external audio encoding.
    cast_initialization_status_ = STATUS_AUDIO_UNINITIALIZED;
  }

  media::cast::CastTransportRtpConfig transport_config;
  transport_config.ssrc = audio_config.ssrc;
  transport_config.feedback_ssrc = audio_config.incoming_feedback_ssrc;
  transport_config.rtp_payload_type = audio_config.rtp_payload_type;
  // TODO(miu): AudioSender needs to be like VideoSender in providing an upper
  // limit on the number of in-flight frames.
  transport_config.stored_frames = max_unacked_frames_;
  transport_config.aes_key = audio_config.aes_key;
  transport_config.aes_iv_mask = audio_config.aes_iv_mask;

  transport_sender->InitializeAudio(
      transport_config,
      base::Bind(&AudioSender::OnReceivedCastFeedback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&AudioSender::OnMeasuredRoundTripTime,
                 weak_factory_.GetWeakPtr()));
}

AudioSender::~AudioSender() {}

void AudioSender::InsertAudio(scoped_ptr<AudioBus> audio_bus,
                              const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (cast_initialization_status_ != STATUS_AUDIO_INITIALIZED) {
    NOTREACHED();
    return;
  }
  DCHECK(audio_encoder_.get()) << "Invalid internal state";

  // TODO(miu): An |audio_bus| that represents more duration than a single
  // frame's duration can defeat our logic here, causing too much data to become
  // enqueued.  This will be addressed in a soon-upcoming change.
  if (ShouldDropNextFrame(recorded_time)) {
    VLOG(1) << "Dropping frame due to too many frames currently in-flight.";
    return;
  }

  samples_in_encoder_ += audio_bus->frames();

  audio_encoder_->InsertAudio(audio_bus.Pass(), recorded_time);
}

int AudioSender::GetNumberOfFramesInEncoder() const {
  // Note: It's possible for a partial frame to be in the encoder, but returning
  // the floor() is good enough for the "design limit" check in FrameSender.
  return samples_in_encoder_ / audio_encoder_->GetSamplesPerFrame();
}

void AudioSender::OnAck(uint32 frame_id) {
}

void AudioSender::OnEncodedAudioFrame(
    int encoder_bitrate,
    scoped_ptr<EncodedFrame> encoded_frame,
    int samples_skipped) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  samples_in_encoder_ -= audio_encoder_->GetSamplesPerFrame() + samples_skipped;
  DCHECK_GE(samples_in_encoder_, 0);

  SendEncodedFrame(encoder_bitrate, encoded_frame.Pass());
}

}  // namespace cast
}  // namespace media
