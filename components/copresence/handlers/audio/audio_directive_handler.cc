// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/copresence/mediums/audio/audio_player.h"
#include "components/copresence/mediums/audio/audio_recorder.h"
#include "components/copresence/proto/data.pb.h"
#include "media/base/audio_bus.h"

namespace copresence {

// Public methods.

AudioDirectiveHandler::AudioDirectiveHandler(
    const AudioRecorder::DecodeSamplesCallback& decode_cb,
    const AudioDirectiveList::EncodeTokenCallback& encode_cb)
    : directive_list_inaudible_(
          encode_cb,
          base::Bind(&AudioDirectiveHandler::ExecuteNextTransmit,
                     base::Unretained(this)),
          false),
      directive_list_audible_(
          encode_cb,
          base::Bind(&AudioDirectiveHandler::ExecuteNextTransmit,
                     base::Unretained(this)),
          true),
      player_(NULL),
      recorder_(NULL),
      decode_cb_(decode_cb) {
}

AudioDirectiveHandler::~AudioDirectiveHandler() {
  if (player_)
    player_->Finalize();
  if (recorder_)
    recorder_->Finalize();
}

void AudioDirectiveHandler::Initialize() {
  player_ = new AudioPlayer();
  player_->Initialize();

  recorder_ = new AudioRecorder(decode_cb_);
  recorder_->Initialize();
}

void AudioDirectiveHandler::AddInstruction(const TokenInstruction& instruction,
                                           base::TimeDelta ttl) {
  switch (instruction.token_instruction_type()) {
    case TRANSMIT:
      DVLOG(2) << "Audio Transmit Directive received. Token: "
               << instruction.token_id()
               << " with TTL=" << ttl.InMilliseconds();
      // TODO(rkc): Fill in the op_id once we get it from the directive.
      switch (instruction.medium()) {
        case AUDIO_ULTRASOUND_PASSBAND:
          directive_list_inaudible_.AddTransmitDirective(
              instruction.token_id(), std::string(), ttl);
          break;
        case AUDIO_AUDIBLE_DTMF:
          directive_list_audible_.AddTransmitDirective(
              instruction.token_id(), std::string(), ttl);
          break;
        default:
          NOTREACHED();
      }
      break;
    case RECEIVE:
      DVLOG(2) << "Audio Receive Directive received. TTL="
               << ttl.InMilliseconds();
      // TODO(rkc): Fill in the op_id once we get it from the directive.
      switch (instruction.medium()) {
        case AUDIO_ULTRASOUND_PASSBAND:
          directive_list_inaudible_.AddReceiveDirective(std::string(), ttl);
          break;
        case AUDIO_AUDIBLE_DTMF:
          directive_list_audible_.AddReceiveDirective(std::string(), ttl);
          break;
        default:
          NOTREACHED();
      }
      break;
    case UNKNOWN_TOKEN_INSTRUCTION_TYPE:
    default:
      LOG(WARNING) << "Unknown Audio Transmit Directive received.";
  }
  // ExecuteNextTransmit will be called by directive_list_ when Add is done.
  ExecuteNextReceive();
}

// Protected methods.

void AudioDirectiveHandler::PlayAudio(
    const scoped_refptr<media::AudioBusRefCounted>& samples,
    base::TimeDelta duration) {
  player_->Play(samples);
  stop_playback_timer_.Start(
      FROM_HERE, duration, this, &AudioDirectiveHandler::StopPlayback);
}

void AudioDirectiveHandler::RecordAudio(base::TimeDelta duration) {
  recorder_->Record();
  stop_recording_timer_.Start(
      FROM_HERE, duration, this, &AudioDirectiveHandler::StopRecording);
}

// Private methods.

void AudioDirectiveHandler::StopPlayback() {
  player_->Stop();
  DVLOG(2) << "Done playing audio.";
  ExecuteNextTransmit();
}

void AudioDirectiveHandler::StopRecording() {
  recorder_->Stop();
  DVLOG(2) << "Done recording audio.";
  ExecuteNextReceive();
}

void AudioDirectiveHandler::ExecuteNextTransmit() {
  scoped_ptr<AudioDirective> audible_transmit(
      directive_list_audible_.GetNextTransmit());
  scoped_ptr<AudioDirective> inaudible_transmit(
      directive_list_inaudible_.GetNextTransmit());

  if (inaudible_transmit) {
    PlayAudio(inaudible_transmit->samples,
              inaudible_transmit->end_time - base::Time::Now());
  }
  if (audible_transmit) {
    PlayAudio(audible_transmit->samples,
              audible_transmit->end_time - base::Time::Now());
  }
}

void AudioDirectiveHandler::ExecuteNextReceive() {
  scoped_ptr<AudioDirective> audible_receive(
      directive_list_audible_.GetNextReceive());
  scoped_ptr<AudioDirective> inaudible_receive(
      directive_list_inaudible_.GetNextReceive());

  base::TimeDelta record_duration;
  if (inaudible_receive)
    record_duration = inaudible_receive->end_time - base::Time::Now();
  if (audible_receive) {
    record_duration = std::max(record_duration,
                               audible_receive->end_time - base::Time::Now());
  }

  if (record_duration > base::TimeDelta::FromSeconds(0))
    RecordAudio(record_duration);
}

}  // namespace copresence
