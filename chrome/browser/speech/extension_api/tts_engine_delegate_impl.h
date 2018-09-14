// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_DELEGATE_IMPL_H_

#include <vector>

#include "chrome/browser/speech/tts_engine_delegate.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "components/keyed_service/core/keyed_service.h"

// This class represents all of the TTS Extension engines for a given
// BrowserContext. It implements the TtsEngineDelegate interface used by
// TtsController.
class TtsEngineDelegateImpl : public TtsEngineDelegate, public KeyedService {
 public:
  // Overridden from TtsEngineDelegate:
  void GetVoices(content::BrowserContext* browser_context,
                 std::vector<VoiceData>* out_voices) override;
  void Speak(Utterance* utterance, const VoiceData& voice) override;
  void Stop(Utterance* utterance) override;
  void Pause(Utterance* utterance) override;
  void Resume(Utterance* utterance) override;
  bool LoadBuiltInTtsExtension() override;

  // Overridden from KeyedService:
  void Shutdown() override;

 private:
  friend class TtsEngineDelegateFactoryImpl;
  explicit TtsEngineDelegateImpl(content::BrowserContext* browser_context);

  content::BrowserContext* browser_context_;
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_DELEGATE_IMPL_H_
