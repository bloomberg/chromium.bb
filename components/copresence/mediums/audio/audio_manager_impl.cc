// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/mediums/audio/audio_manager_impl.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "components/copresence/mediums/audio/audio_player_impl.h"
#include "components/copresence/mediums/audio/audio_recorder_impl.h"
#include "components/copresence/public/copresence_constants.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

namespace copresence {

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

// Public methods.

AudioManagerImpl::AudioManagerImpl() : recorder_(nullptr) {
  // TODO(rkc): Move all of these into initializer lists once it is allowed.
  playing_[AUDIBLE] = false;
  playing_[INAUDIBLE] = false;
  recording_[AUDIBLE] = false;
  recording_[INAUDIBLE] = false;

  player_[AUDIBLE] = nullptr;
  player_[INAUDIBLE] = nullptr;
}

void AudioManagerImpl::Initialize(const DecodeSamplesCallback& decode_cb,
                                  const EncodeTokenCallback& encode_cb) {
  samples_cache_.resize(2);
  samples_cache_[AUDIBLE] = new SamplesMap(
      base::TimeDelta::FromMilliseconds(kSampleExpiryTimeMs), kMaxSamples);
  samples_cache_[INAUDIBLE] = new SamplesMap(
      base::TimeDelta::FromMilliseconds(kSampleExpiryTimeMs), kMaxSamples);

  decode_cb_ = decode_cb;
  encode_cb_ = encode_cb;

  if (!player_[AUDIBLE])
    player_[AUDIBLE] = new AudioPlayerImpl();
  player_[AUDIBLE]->Initialize();

  if (!player_[INAUDIBLE])
    player_[INAUDIBLE] = new AudioPlayerImpl();
  player_[INAUDIBLE]->Initialize();

  decode_cancelable_cb_.Reset(base::Bind(
      &AudioManagerImpl::DecodeSamplesConnector, base::Unretained(this)));
  if (!recorder_)
    recorder_ = new AudioRecorderImpl();
  recorder_->Initialize(decode_cancelable_cb_.callback());
}

AudioManagerImpl::~AudioManagerImpl() {
  if (player_[AUDIBLE])
    player_[AUDIBLE]->Finalize();
  if (player_[INAUDIBLE])
    player_[INAUDIBLE]->Finalize();
  if (recorder_)
    recorder_->Finalize();
}

void AudioManagerImpl::StartPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  playing_[type] = true;
  // If we don't have our token encoded yet, this check will be false, for now.
  // Once our token is encoded, OnTokenEncoded will call UpdateToken, which
  // will call this code again (if we're still supposed to be playing).
  if (samples_cache_[type]->HasKey(token_[type]) &&
      !player_[type]->IsPlaying()) {
    DCHECK(!token_[type].empty());
    player_[type]->Play(samples_cache_[type]->GetValue(token_[type]));
  }
}

void AudioManagerImpl::StopPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  playing_[type] = false;
  if (player_[type]->IsPlaying())
    player_[type]->Stop();
}

void AudioManagerImpl::StartRecording(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  recording_[type] = true;
  if (!recorder_->IsRecording())
    recorder_->Record();
}

void AudioManagerImpl::StopRecording(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  recording_[type] = false;
  if (recorder_->IsRecording())
    recorder_->Stop();
}

void AudioManagerImpl::SetToken(AudioType type,
                                const std::string& url_unsafe_token) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  std::string token = FromUrlSafe(url_unsafe_token);
  if (!samples_cache_[type]->HasKey(token)) {
    // We're destructed by the destruction chain of
    // RpcHandler->DirectiveHandler->AudioDirectiveHandler. The RpcHandler
    // unsets any callbacks that were set on the Whispernet client before it
    // destructs, unsetting this callback too - making unretained safe to use.
    encode_cb_.Run(
        token,
        type,
        base::Bind(&AudioManagerImpl::OnTokenEncoded, base::Unretained(this)));
  } else {
    UpdateToken(type, token);
  }
}

const std::string AudioManagerImpl::GetToken(AudioType type) {
  return token_[type];
}

bool AudioManagerImpl::IsRecording(AudioType type) {
  return recording_[type];
}

bool AudioManagerImpl::IsPlaying(AudioType type) {
  return playing_[type];
}

// Private methods.

void AudioManagerImpl::OnTokenEncoded(
    const std::string& token,
    AudioType type,
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  samples_cache_[type]->Add(token, samples);
  UpdateToken(type, token);
}

void AudioManagerImpl::UpdateToken(AudioType type, const std::string& token) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  if (token_[type] == token)
    return;

  // Update token.
  token_[type] = token;

  // If we are supposed to be playing this token type at this moment, switch
  // out playback with the new samples.
  if (playing_[type]) {
    if (player_[type]->IsPlaying())
      player_[type]->Stop();
    StartPlaying(type);
  }
}

void AudioManagerImpl::DecodeSamplesConnector(const std::string& samples) {
  AudioType decode_type = AUDIO_TYPE_UNKNOWN;

  if (recording_[AUDIBLE] && recording_[INAUDIBLE])
    decode_type = BOTH;
  else if (recording_[AUDIBLE])
    decode_type = AUDIBLE;
  else if (recording_[INAUDIBLE])
    decode_type = INAUDIBLE;

  decode_cb_.Run(decode_type, samples);
}

}  // namespace copresence
