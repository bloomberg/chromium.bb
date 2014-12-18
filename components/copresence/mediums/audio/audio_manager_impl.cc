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
#include "base/time/time.h"
#include "components/copresence/mediums/audio/audio_player_impl.h"
#include "components/copresence/mediums/audio/audio_recorder_impl.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/public/whispernet_client.h"
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
const int kTokenTimeoutMs = 2000;

}  // namespace

// Public methods.

AudioManagerImpl::AudioManagerImpl()
    : whispernet_client_(nullptr), recorder_(nullptr) {
  // TODO(rkc): Move all of these into initializer lists once it is allowed.
  should_be_playing_[AUDIBLE] = false;
  should_be_playing_[INAUDIBLE] = false;
  should_be_recording_[AUDIBLE] = false;
  should_be_recording_[INAUDIBLE] = false;

  player_[AUDIBLE] = nullptr;
  player_[INAUDIBLE] = nullptr;
}

void AudioManagerImpl::Initialize(WhispernetClient* whispernet_client,
                                  const TokensCallback& tokens_cb) {
  samples_cache_.resize(2);
  samples_cache_[AUDIBLE] = new SamplesMap(
      base::TimeDelta::FromMilliseconds(kSampleExpiryTimeMs), kMaxSamples);
  samples_cache_[INAUDIBLE] = new SamplesMap(
      base::TimeDelta::FromMilliseconds(kSampleExpiryTimeMs), kMaxSamples);

  DCHECK(whispernet_client);
  whispernet_client_ = whispernet_client;
  tokens_cb_ = tokens_cb;

  // These will be unregistered on destruction, so unretained is safe to use.
  whispernet_client_->RegisterTokensCallback(
      base::Bind(&AudioManagerImpl::OnTokensFound, base::Unretained(this)));
  whispernet_client_->RegisterSamplesCallback(
      base::Bind(&AudioManagerImpl::OnTokenEncoded, base::Unretained(this)));

  if (!player_[AUDIBLE])
    player_[AUDIBLE] = new AudioPlayerImpl();
  player_[AUDIBLE]->Initialize();

  if (!player_[INAUDIBLE])
    player_[INAUDIBLE] = new AudioPlayerImpl();
  player_[INAUDIBLE]->Initialize();

  decode_cancelable_cb_.Reset(base::Bind(&WhispernetClient::DecodeSamples,
                                         base::Unretained(whispernet_client_),
                                         BOTH));
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

  // Whispernet initialization may never have completed.
  if (whispernet_client_) {
    whispernet_client_->RegisterTokensCallback(TokensCallback());
    whispernet_client_->RegisterSamplesCallback(SamplesCallback());
  }
}

void AudioManagerImpl::StartPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_playing_[type] = true;
  // If we don't have our token encoded yet, this check will be false, for now.
  // Once our token is encoded, OnTokenEncoded will call UpdateToken, which
  // will call this code again (if we're still supposed to be playing).
  if (samples_cache_[type]->HasKey(playing_token_[type])) {
    DCHECK(!playing_token_[type].empty());
    started_playing_[type] = base::Time::Now();
    player_[type]->Play(samples_cache_[type]->GetValue(playing_token_[type]));
    // If we're playing, we always record to hear what we are playing.
    recorder_->Record();
  }
}

void AudioManagerImpl::StopPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_playing_[type] = false;
  player_[type]->Stop();
  // If we were only recording to hear our own played tokens, stop.
  if (!should_be_recording_[AUDIBLE] && !should_be_recording_[INAUDIBLE])
    recorder_->Stop();
}

void AudioManagerImpl::StartRecording(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_recording_[type] = true;
  recorder_->Record();
}

void AudioManagerImpl::StopRecording(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_recording_[type] = false;
  recorder_->Stop();
}

void AudioManagerImpl::SetToken(AudioType type,
                                const std::string& url_unsafe_token) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  std::string token = FromUrlSafe(url_unsafe_token);
  if (!samples_cache_[type]->HasKey(token)) {
    whispernet_client_->EncodeToken(token, type);
  } else {
    UpdateToken(type, token);
  }
}

const std::string AudioManagerImpl::GetToken(AudioType type) {
  return should_be_playing_[type] ? playing_token_[type] : "";
}

bool AudioManagerImpl::IsPlayingTokenHeard(AudioType type) {
  base::TimeDelta tokenTimeout =
      base::TimeDelta::FromMilliseconds(kTokenTimeoutMs);

  // This is a bit of a hack. If we haven't been playing long enough,
  // return true to avoid tripping an audio fail alarm.
  if (base::Time::Now() - started_playing_[type] < tokenTimeout)
    return true;

  return base::Time::Now() - heard_own_token_[type] < tokenTimeout;
}

// Private methods.

void AudioManagerImpl::OnTokenEncoded(
    AudioType type,
    const std::string& token,
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  samples_cache_[type]->Add(token, samples);
  UpdateToken(type, token);
}

void AudioManagerImpl::OnTokensFound(const std::vector<AudioToken>& tokens) {
  std::vector<AudioToken> tokens_to_report;
  for (const auto& token : tokens) {
    AudioType type = token.audible ? AUDIBLE : INAUDIBLE;
    if (playing_token_[type] == token.token)
      heard_own_token_[type] = base::Time::Now();

    if (should_be_recording_[AUDIBLE] && token.audible) {
      tokens_to_report.push_back(token);
    } else if (should_be_recording_[INAUDIBLE] && !token.audible) {
      tokens_to_report.push_back(token);
    }
  }

  if (!tokens_to_report.empty())
    tokens_cb_.Run(tokens_to_report);
}

void AudioManagerImpl::UpdateToken(AudioType type, const std::string& token) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  if (playing_token_[type] == token)
    return;

  // Update token.
  playing_token_[type] = token;

  // If we are supposed to be playing this token type at this moment, switch
  // out playback with the new samples.
  if (should_be_playing_[type])
    RestartPlaying(type);
}

void AudioManagerImpl::RestartPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  // We should already have this token in the cache. This function is not
  // called from anywhere except update token and only once we have our samples
  // in the cache.
  DCHECK(samples_cache_[type]->HasKey(playing_token_[type]));

  started_playing_[type] = base::Time::Now();
  player_[type]->Stop();
  player_[type]->Play(samples_cache_[type]->GetValue(playing_token_[type]));
  // If we're playing, we always record to hear what we are playing.
  recorder_->Record();
}

}  // namespace copresence
