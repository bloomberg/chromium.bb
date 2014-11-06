// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_TEST_STUB_WHISPERNET_CLIENT_H_
#define COMPONENTS_COPRESENCE_TEST_STUB_WHISPERNET_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"

#include "components/copresence/public/whispernet_client.h"

namespace copresence {

// An empty WhispernetClient for testing.
class StubWhispernetClient final : public WhispernetClient {
 public:
  StubWhispernetClient();
  ~StubWhispernetClient() override;

  void Initialize(const SuccessCallback& /* init_cb */) override {}
  void Shutdown() override {}
  void EncodeToken(const std::string& token, AudioType type) override;
  void DecodeSamples(AudioType type, const std::string& samples) override;
  void DetectBroadcast() override {}
  void RegisterTokensCallback(const TokensCallback& tokens_cb) override;
  void RegisterSamplesCallback(const SamplesCallback& samples_cb) override;
  void RegisterDetectBroadcastCallback(
      const SuccessCallback& /* db_cb */) override {}
  TokensCallback GetTokensCallback() override;
  SamplesCallback GetSamplesCallback() override;
  SuccessCallback GetDetectBroadcastCallback() override;
  SuccessCallback GetInitializedCallback() override;

 private:
  TokensCallback tokens_cb_;
  SamplesCallback samples_cb_;
  std::vector<AudioToken> tokens_;
  scoped_refptr<media::AudioBusRefCounted> samples_;

  DISALLOW_COPY_AND_ASSIGN(StubWhispernetClient);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_TEST_STUB_WHISPERNET_CLIENT_H_
