// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_system.h"

// Chrome OS doesn't have native TTS, instead it includes a built-in
// component extension that provides speech synthesis. This class includes
// an implementation of LoadBuiltInTtsExtension and dummy implementations of
// everything else.
class TtsPlatformImplChromeOs
    : public TtsPlatformImpl {
 public:
  // TtsPlatformImpl overrides:
  virtual bool PlatformImplAvailable() OVERRIDE {
    return false;
  }

  virtual bool LoadBuiltInTtsExtension(Profile* profile) OVERRIDE {
    // Check to see if the engine was previously loaded.
    if (TtsEngineExtensionObserver::GetInstance(profile)->SawExtensionLoad(
            extension_misc::kSpeechSynthesisExtensionId, true)) {
      return false;
    }

    // Load the component extension into this profile.
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    DCHECK(extension_service);
    extension_service->component_loader()
        ->AddChromeOsSpeechSynthesisExtension();
    return true;
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
