// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_platform_impl.h"

#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"

#include <string>

namespace content {

// static
TtsPlatform* TtsPlatform::GetInstance() {
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // Chrome and Android TTS platforms have chrome/ dependencies.
  return GetContentClient()->browser()->GetTtsPlatform();
#elif defined(OS_FUCHSIA)
  // There is no platform TTS definition for Fuchsia.
  return nullptr;
#else
  return TtsPlatformImpl::GetInstance();
#endif
}

bool TtsPlatformImpl::LoadBuiltInTtsExtension(BrowserContext* browser_context) {
  return false;
}

void TtsPlatformImpl::WillSpeakUtteranceWithVoice(const Utterance* utterance,
                                                  const VoiceData& voice_data) {
}

std::string TtsPlatformImpl::GetError() {
  return error_;
}

void TtsPlatformImpl::ClearError() {
  error_ = std::string();
}

void TtsPlatformImpl::SetError(const std::string& error) {
  error_ = error;
}

}  // namespace content