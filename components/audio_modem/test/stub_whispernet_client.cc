// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/test/stub_whispernet_client.h"

namespace audio_modem {

StubWhispernetClient::StubWhispernetClient(
    scoped_refptr<media::AudioBusRefCounted> samples,
    const std::vector<AudioToken>& tokens)
    : samples_(samples),
      tokens_(tokens) {
}

StubWhispernetClient::~StubWhispernetClient() {}

void StubWhispernetClient::Initialize(const SuccessCallback& init_callback) {}

void StubWhispernetClient::EncodeToken(const std::string& token,
                                       AudioType type,
                                       const TokenParameters token_params[2]) {
  if (!samples_cb_.is_null())
    samples_cb_.Run(type, token, samples_);
}

void StubWhispernetClient::DecodeSamples(
    AudioType type,
    const std::string& samples,
    const TokenParameters token_params[2]) {
  if (!tokens_cb_.is_null())
    tokens_cb_.Run(tokens_);
}

void StubWhispernetClient::RegisterTokensCallback(
    const TokensCallback& tokens_cb) {
  tokens_cb_ = tokens_cb;
}

void StubWhispernetClient::RegisterSamplesCallback(
    const SamplesCallback& samples_cb) {
  samples_cb_ = samples_cb;
}

TokensCallback StubWhispernetClient::GetTokensCallback() {
  return tokens_cb_;
}

SamplesCallback StubWhispernetClient::GetSamplesCallback() {
  return samples_cb_;
}

SuccessCallback StubWhispernetClient::GetInitializedCallback() {
  return SuccessCallback();
}

}  // namespace audio_modem
