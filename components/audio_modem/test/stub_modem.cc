// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/test/stub_modem.h"

#include "base/base64.h"
#include "base/logging.h"

namespace audio_modem {

StubModem::StubModem() {
  playing_[AUDIBLE] = false;
  playing_[INAUDIBLE] = false;
  recording_[AUDIBLE] = false;
  recording_[INAUDIBLE] = false;
}

StubModem::~StubModem() {}

void StubModem::Initialize(WhispernetClient* whispernet_client,
                           const TokensCallback& tokens_cb) {
  tokens_callback_ = tokens_cb;
}

void StubModem::StartPlaying(AudioType type) {
  playing_[type] = true;
}

void StubModem::StopPlaying(AudioType type) {
  playing_[type] = false;
}

void StubModem::StartRecording(AudioType type) {
  recording_[type] = true;
}

void StubModem::StopRecording(AudioType type) {
  recording_[type] = false;
}

const std::string StubModem::GetToken(AudioType type) const {
  return std::string();
}

bool StubModem::IsPlayingTokenHeard(AudioType type) const {
  return false;
}

bool StubModem::IsRecording(AudioType type) const {
  return recording_[type];
}

bool StubModem::IsPlaying(AudioType type) const {
  return playing_[type];
}

void StubModem::DeliverTokens(const std::vector<AudioToken>& tokens) {
  std::vector<AudioToken> encoded_tokens;
  for (const AudioToken& token : tokens) {
    std::string encoded_token;
    base::Base64Encode(token.token, & encoded_token);
    encoded_tokens.push_back(AudioToken(encoded_token, token.audible));
  }
  DCHECK_EQ(tokens.size(), encoded_tokens.size());
  tokens_callback_.Run(encoded_tokens);
}

}  // namespace audio_modem
