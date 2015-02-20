// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_TEST_STUB_WHISPERNET_CLIENT_H_
#define COMPONENTS_AUDIO_MODEM_TEST_STUB_WHISPERNET_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/audio_modem/public/whispernet_client.h"
#include "media/base/audio_bus.h"

namespace audio_modem {

// A simple WhispernetClient for testing.
class StubWhispernetClient final : public WhispernetClient {
 public:
  // Constructor. |samples| and |tokens|, if specified,
  // will be returned for any encoding and decoding requests.
  StubWhispernetClient(
      scoped_refptr<media::AudioBusRefCounted> samples =
          scoped_refptr<media::AudioBusRefCounted>(),
      const std::vector<AudioToken>& tokens = std::vector<AudioToken>());

  ~StubWhispernetClient() override;

  void Initialize(const SuccessCallback& init_callback) override;

  void EncodeToken(const std::string& token,
                   AudioType type,
                   const TokenParameters token_params[2]) override;
  void DecodeSamples(AudioType type,
                     const std::string& samples,
                     const TokenParameters token_params[2]) override;

  void RegisterTokensCallback(const TokensCallback& tokens_cb) override;
  void RegisterSamplesCallback(const SamplesCallback& samples_cb) override;

  TokensCallback GetTokensCallback() override;
  SamplesCallback GetSamplesCallback() override;
  SuccessCallback GetInitializedCallback() override;

 private:
  TokensCallback tokens_cb_;
  SamplesCallback samples_cb_;
  scoped_refptr<media::AudioBusRefCounted> samples_;
  std::vector<AudioToken> tokens_;

  DISALLOW_COPY_AND_ASSIGN(StubWhispernetClient);
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_TEST_STUB_WHISPERNET_CLIENT_H_
