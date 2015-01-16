// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_platform.h"

// Chrome OS doesn't have native TTS, instead it includes a built-in
// component extension that provides speech synthesis. This class includes
// an implementation of LoadBuiltInTtsExtension and dummy implementations of
// everything else.

class TtsPlatformImplChromeOs : public TtsPlatformImpl {
 public:
  // TtsPlatformImpl overrides:
  bool PlatformImplAvailable() override { return false; }

  bool LoadBuiltInTtsExtension(
      content::BrowserContext* browser_context) override {
    TtsEngineDelegate* tts_engine_delegate =
        TtsController::GetInstance()->GetTtsEngineDelegate();
    if (tts_engine_delegate)
      return tts_engine_delegate->LoadBuiltInTtsExtension(browser_context);
    return false;
  }

  bool Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params) override {
    return false;
  }

  bool StopSpeaking() override { return false; }

  void Pause() override {}

  void Resume() override {}

  bool IsSpeaking() override { return false; }

  void GetVoices(std::vector<VoiceData>* out_voices) override {}

  // Get the single instance of this class.
  static TtsPlatformImplChromeOs* GetInstance();

 private:
  TtsPlatformImplChromeOs() {}
  ~TtsPlatformImplChromeOs() override {}

  friend struct DefaultSingletonTraits<TtsPlatformImplChromeOs>;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplChromeOs);
};

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplChromeOs::GetInstance();
}

// static
TtsPlatformImplChromeOs*
TtsPlatformImplChromeOs::GetInstance() {
  return Singleton<TtsPlatformImplChromeOs>::get();
}
