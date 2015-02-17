// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COPRESENCE_CHROME_WHISPERNET_CLIENT_H_
#define CHROME_BROWSER_COPRESENCE_CHROME_WHISPERNET_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/copresence/chrome_whispernet_config.h"
#include "components/audio_modem/public/whispernet_client.h"

namespace content {
class BrowserContext;
}

namespace extensions {

struct Event;
class EventRouter;

namespace api {
namespace copresence_private {
struct AudioParameters;
}
}

}  // namespace extensions

namespace media {
class AudioBusRefCounted;
}

// This class is responsible for communication with our whispernet_proxy
// extension that talks to the whispernet audio library.
class ChromeWhispernetClient final : public audio_modem::WhispernetClient {
 public:
  // The browser context needs to outlive this class.
  explicit ChromeWhispernetClient(content::BrowserContext* browser_context);
  ~ChromeWhispernetClient() override;

  // WhispernetClient overrides:
  void Initialize(const audio_modem::SuccessCallback& init_callback) override;
  void EncodeToken(const std::string& token_str,
                   audio_modem::AudioType type) override;
  void DecodeSamples(audio_modem::AudioType type,
                     const std::string& samples,
                     const size_t token_length[2]) override;
  void DetectBroadcast() override;
  void RegisterTokensCallback(
      const audio_modem::TokensCallback& tokens_callback) override;
  void RegisterSamplesCallback(
      const audio_modem::SamplesCallback& samples_callback) override;
  void RegisterDetectBroadcastCallback(
      const audio_modem::SuccessCallback& db_callback) override;

  audio_modem::TokensCallback GetTokensCallback() override;
  audio_modem::SamplesCallback GetSamplesCallback() override;
  audio_modem::SuccessCallback GetDetectBroadcastCallback() override;
  audio_modem::SuccessCallback GetInitializedCallback() override;

  static const char kWhispernetProxyExtensionId[];

 private:
  // Fire an event to configure whispernet with the given audio parameters.
  void AudioConfiguration(const AudioParamData& params);

  void SendEventIfLoaded(scoped_ptr<extensions::Event> event);

  // This gets called twice; once when the proxy extension loads, the second
  // time when we have initialized the proxy extension's encoder and decoder.
  void OnExtensionLoaded(bool success);

  content::BrowserContext* const browser_context_;
  extensions::EventRouter* const event_router_;

  audio_modem::SuccessCallback extension_loaded_callback_;
  audio_modem::SuccessCallback init_callback_;

  audio_modem::TokensCallback tokens_callback_;
  audio_modem::SamplesCallback samples_callback_;
  audio_modem::SuccessCallback db_callback_;

  ScopedVector<extensions::Event> queued_events_;
  bool extension_loaded_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWhispernetClient);
};

#endif  // CHROME_BROWSER_COPRESENCE_CHROME_WHISPERNET_CLIENT_H_
