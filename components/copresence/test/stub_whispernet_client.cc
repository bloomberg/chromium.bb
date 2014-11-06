// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/test/stub_whispernet_client.h"

#include "components/copresence/test/audio_test_support.h"
#include "media/base/audio_bus.h"

namespace copresence {

StubWhispernetClient::StubWhispernetClient() {
  tokens_.push_back(AudioToken("abcdef", true));
  tokens_.push_back(AudioToken("123456", false));
  samples_ = CreateRandomAudioRefCounted(0x123, 1, 0x321);
}

StubWhispernetClient::~StubWhispernetClient() {
}

void StubWhispernetClient::EncodeToken(const std::string& token,
                                       AudioType type) {
  if (!samples_cb_.is_null())
    samples_cb_.Run(type, token, samples_);
}

void StubWhispernetClient::DecodeSamples(AudioType type,
                                         const std::string& samples) {
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

SuccessCallback StubWhispernetClient::GetDetectBroadcastCallback() {
  return SuccessCallback();
}

SuccessCallback StubWhispernetClient::GetInitializedCallback() {
  return SuccessCallback();
}

}  // namespace copresence
