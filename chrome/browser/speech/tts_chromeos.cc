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
  virtual bool PlatformImplAvailable() OVERRIDE {
    return false;
  }

  virtual bool LoadBuiltInTtsExtension(
      content::BrowserContext* browser_context) OVERRIDE {
    TtsEngineDelegate* tts_engine_delegate =
        TtsController::GetInstance()->GetTtsEngineDelegate();
    if (tts_engine_delegate)
      return tts_engine_delegate->LoadBuiltInTtsExtension(browser_context);
    return false;
  }

  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params) OVERRIDE {
    return false;
  }

  virtual bool StopSpeaking() OVERRIDE {
    return false;
  }

  virtual void Pause() OVERRIDE {}

  virtual void Resume() OVERRIDE {}

  virtual bool IsSpeaking() OVERRIDE {
    return false;
  }

  virtual void GetVoices(std::vector<VoiceData>* out_voices) OVERRIDE {
  }

  // Get the single instance of this class.
  static TtsPlatformImplChromeOs* GetInstance();

 private:
  TtsPlatformImplChromeOs() {}
  virtual ~TtsPlatformImplChromeOs() {}

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
