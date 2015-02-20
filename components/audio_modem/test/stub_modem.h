// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_TEST_STUB_MODEM_H_
#define COMPONENTS_AUDIO_MODEM_TEST_STUB_MODEM_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/audio_modem/public/modem.h"

namespace audio_modem {

class StubModem final : public Modem {
 public:
  StubModem();
  ~StubModem() override;

  // AudioManager overrides:
  void Initialize(WhispernetClient* whispernet_client,
                  const TokensCallback& tokens_cb) override;
  void StartPlaying(AudioType type) override;
  void StopPlaying(AudioType type) override;
  void StartRecording(AudioType type) override;
  void StopRecording(AudioType type) override;
  void SetToken(AudioType type, const std::string& url_unsafe_token) override {}
  const std::string GetToken(AudioType type) const override;
  bool IsPlayingTokenHeard(AudioType type) const override;
  void SetTokenParams(AudioType type, const TokenParameters& params) override {}

  bool IsRecording(AudioType type) const;
  bool IsPlaying(AudioType type) const;

  // Encodes tokens as base64 and then delivers them.
  void DeliverTokens(const std::vector<AudioToken>& tokens);

 private:
  // Indexed using enum AudioType.
  bool playing_[2];
  bool recording_[2];

  TokensCallback tokens_callback_;

  DISALLOW_COPY_AND_ASSIGN(StubModem);
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_TEST_STUB_MODEM_H_
