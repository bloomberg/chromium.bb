// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/copresence/mediums/audio/audio_player.h"
#include "components/copresence/mediums/audio/audio_recorder.h"
#include "components/copresence/proto/data.pb.h"
#include "media/base/audio_bus.h"

namespace {

// UrlSafe is defined as:
// '/' represented by a '_' and '+' represented by a '-'
// TODO(rkc): Move this processing to the whispernet wrapper.
std::string FromUrlSafe(std::string token) {
  base::ReplaceChars(token, "-", "+", &token);
  base::ReplaceChars(token, "_", "/", &token);
  return token;
}

const int kSampleExpiryTimeMs = 60 * 60 * 1000;  // 60 minutes.
const int kMaxSamples = 10000;

}  // namespace

namespace copresence {

// Public methods.

AudioDirectiveHandler::AudioDirectiveHandler(
    const AudioRecorder::DecodeSamplesCallback& decode_cb,
    const AudioDirectiveHandler::EncodeTokenCallback& encode_cb)
    : player_audible_(NULL),
      player_inaudible_(NULL),
      recorder_(NULL),
      decode_cb_(decode_cb),
      encode_cb_(encode_cb),
      samples_cache_audible_(
          base::TimeDelta::FromMilliseconds(kSampleExpiryTimeMs),
          kMaxSamples),
      samples_cache_inaudible_(
          base::TimeDelta::FromMilliseconds(kSampleExpiryTimeMs),
          kMaxSamples) {
}

AudioDirectiveHandler::~AudioDirectiveHandler() {
  if (player_audible_)
    player_audible_->Finalize();
  if (player_inaudible_)
    player_inaudible_->Finalize();
  if (recorder_)
    recorder_->Finalize();
}

void AudioDirectiveHandler::Initialize() {
  player_audible_ = new AudioPlayer();
  player_audible_->Initialize();

  player_inaudible_ = new AudioPlayer();
  player_inaudible_->Initialize();

  recorder_ = new AudioRecorder(decode_cb_);
  recorder_->Initialize();
}

void AudioDirectiveHandler::AddInstruction(const TokenInstruction& instruction,
                                           const std::string& op_id,
                                           base::TimeDelta ttl) {
  switch (instruction.token_instruction_type()) {
    case TRANSMIT:
      DVLOG(2) << "Audio Transmit Directive received. Token: "
               << instruction.token_id()
               << " with TTL=" << ttl.InMilliseconds();
      switch (instruction.medium()) {
        case AUDIO_ULTRASOUND_PASSBAND:
          transmits_list_inaudible_.AddDirective(op_id, ttl);
          PlayToken(instruction.token_id(), false);
          break;
        case AUDIO_AUDIBLE_DTMF:
          transmits_list_audible_.AddDirective(op_id, ttl);
          PlayToken(instruction.token_id(), true);
          break;
        default:
          NOTREACHED();
      }
      break;
    case RECEIVE:
      DVLOG(2) << "Audio Receive Directive received. TTL="
               << ttl.InMilliseconds();
      receives_list_.AddDirective(op_id, ttl);
      ProcessNextReceive();
      break;
    case UNKNOWN_TOKEN_INSTRUCTION_TYPE:
    default:
      LOG(WARNING) << "Unknown Audio Transmit Directive received.";
  }
}

void AudioDirectiveHandler::RemoveInstructions(const std::string& op_id) {
  transmits_list_audible_.RemoveDirective(op_id);
  transmits_list_inaudible_.RemoveDirective(op_id);
  receives_list_.RemoveDirective(op_id);

  ProcessNextTransmit();
  ProcessNextReceive();
}

// Private methods.

void AudioDirectiveHandler::ProcessNextTransmit() {
  // If we have an active directive for audible or inaudible audio, ensure that
  // we are playing our respective token; if we do not have a directive, then
  // make sure we aren't playing. This is duplicate code, but for just two
  // elements, it has hard to make a case for processing a loop instead.

  scoped_ptr<AudioDirective> audible_transmit(
      transmits_list_audible_.GetActiveDirective());
  if (audible_transmit && !player_audible_->IsPlaying() &&
      samples_cache_audible_.HasKey(current_token_audible_)) {
    DVLOG(3) << "Playing audible for op_id: " << audible_transmit->op_id;
    player_audible_->Play(
        samples_cache_audible_.GetValue(current_token_audible_));
    stop_audible_playback_timer_.Start(
        FROM_HERE,
        audible_transmit->end_time - base::Time::Now(),
        this,
        &AudioDirectiveHandler::ProcessNextTransmit);
  } else if (!audible_transmit && player_audible_->IsPlaying()) {
    DVLOG(3) << "Stopping audible playback.";
    current_token_audible_.clear();
    stop_audible_playback_timer_.Stop();
    player_audible_->Stop();
  }

  scoped_ptr<AudioDirective> inaudible_transmit(
      transmits_list_inaudible_.GetActiveDirective());
  if (inaudible_transmit && !player_inaudible_->IsPlaying() &&
      samples_cache_inaudible_.HasKey(current_token_inaudible_)) {
    DVLOG(3) << "Playing inaudible for op_id: " << inaudible_transmit->op_id;
    player_inaudible_->Play(
        samples_cache_inaudible_.GetValue(current_token_inaudible_));
    stop_inaudible_playback_timer_.Start(
        FROM_HERE,
        inaudible_transmit->end_time - base::Time::Now(),
        this,
        &AudioDirectiveHandler::ProcessNextTransmit);
  } else if (!inaudible_transmit && player_inaudible_->IsPlaying()) {
    DVLOG(3) << "Stopping inaudible playback.";
    current_token_inaudible_.clear();
    stop_inaudible_playback_timer_.Stop();
    player_inaudible_->Stop();
  }
}

void AudioDirectiveHandler::ProcessNextReceive() {
  scoped_ptr<AudioDirective> receive(receives_list_.GetActiveDirective());

  if (receive && !recorder_->IsRecording()) {
    DVLOG(3) << "Recording for op_id: " << receive->op_id;
    recorder_->Record();
    stop_recording_timer_.Start(FROM_HERE,
                                receive->end_time - base::Time::Now(),
                                this,
                                &AudioDirectiveHandler::ProcessNextReceive);
  } else if (!receive && recorder_->IsRecording()) {
    DVLOG(3) << "Stopping Recording";
    stop_recording_timer_.Stop();
    recorder_->Stop();
  }
}

void AudioDirectiveHandler::PlayToken(const std::string token, bool audible) {
  std::string valid_token = FromUrlSafe(token);

  // If the token has been encoded already, use the cached samples.
  if (audible && samples_cache_audible_.HasKey(valid_token)) {
    current_token_audible_ = token;
    ProcessNextTransmit();
  } else if (!audible && samples_cache_inaudible_.HasKey(valid_token)) {
    current_token_inaudible_ = token;
    ProcessNextTransmit();
  } else {
    // Otherwise, encode the token and then play it.
    encode_cb_.Run(valid_token,
                   audible,
                   base::Bind(&AudioDirectiveHandler::PlayEncodedToken,
                              base::Unretained(this)));
  }
}

void AudioDirectiveHandler::PlayEncodedToken(
    const std::string& token,
    bool audible,
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  DVLOG(3) << "Token " << token << "[audible:" << audible << "] encoded.";
  if (audible) {
    samples_cache_audible_.Add(token, samples);
    current_token_audible_ = token;
    // Force process transmits to pick up the new token.
    if (player_audible_->IsPlaying())
      player_audible_->Stop();
  } else {
    samples_cache_inaudible_.Add(token, samples);
    current_token_inaudible_ = token;
    // Force process transmits to pick up the new token.
    if (player_inaudible_->IsPlaying())
      player_inaudible_->Stop();
  }

  ProcessNextTransmit();
}

}  // namespace copresence
