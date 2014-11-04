// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_TEST_STUB_WHISPERNET_CLIENT_H_
#define COMPONENTS_COPRESENCE_TEST_STUB_WHISPERNET_CLIENT_H_

#include "components/copresence/public/whispernet_client.h"

namespace copresence {

// An empty WhispernetClient for testing.
class StubWhispernetClient final : public WhispernetClient {
 public:
  StubWhispernetClient() {}

  void Initialize(const SuccessCallback& /* init_callback */) override {}
  void Shutdown() override {}
  void EncodeToken(const std::string& /* token */, AudioType /* type */)
      override {}
  void DecodeSamples(AudioType /* type */, const std::string& /* samples */)
      override {}
  void DetectBroadcast() override {}
  void RegisterTokensCallback(
      const TokensCallback& /* tokens_callback */) override {}
  void RegisterSamplesCallback(
      const SamplesCallback& /* samples_callback */) override {}
  void RegisterDetectBroadcastCallback(
      const SuccessCallback& /* db_callback */) override {}
  TokensCallback GetTokensCallback() override;
  SamplesCallback GetSamplesCallback() override;
  SuccessCallback GetDetectBroadcastCallback() override;
  SuccessCallback GetInitializedCallback() override;
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_TEST_STUB_WHISPERNET_CLIENT_H_
