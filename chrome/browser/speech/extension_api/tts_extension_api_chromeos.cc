// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_extension_api_platform.h"
#include "chrome/browser/speech/extension_api/tts_extension_loader_chromeos.h"

// Chrome OS doesn't have native TTS, instead it includes a built-in
// component extension that provides speech synthesis. This class includes
// an implementation of LoadBuiltInTtsExtension and dummy implementations of
// everything else.
class ExtensionTtsPlatformImplChromeOs
    : public ExtensionTtsPlatformImpl {
 public:
  // ExtensionTtsPlatformImpl overrides:
  virtual bool PlatformImplAvailable() OVERRIDE {
    return false;
  }

  virtual bool LoadBuiltInTtsExtension(Profile* profile) OVERRIDE {
    return TtsExtensionLoaderChromeOs::GetInstance(profile)->LoadTtsExtension();
  }

  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params) OVERRIDE {
    return false;
  }

  virtual bool StopSpeaking() OVERRIDE {
    return false;
  }

  virtual bool IsSpeaking() OVERRIDE {
    return false;
  }

  virtual bool SendsEvent(TtsEventType event_type) OVERRIDE {
    return false;
  }

  // Get the single instance of this class.
  static ExtensionTtsPlatformImplChromeOs* GetInstance();

 private:
  ExtensionTtsPlatformImplChromeOs() {}
  virtual ~ExtensionTtsPlatformImplChromeOs() {}

  friend struct DefaultSingletonTraits<ExtensionTtsPlatformImplChromeOs>;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImplChromeOs);
};

// static
ExtensionTtsPlatformImpl* ExtensionTtsPlatformImpl::GetInstance() {
  return ExtensionTtsPlatformImplChromeOs::GetInstance();
}

// static
ExtensionTtsPlatformImplChromeOs*
ExtensionTtsPlatformImplChromeOs::GetInstance() {
  return Singleton<ExtensionTtsPlatformImplChromeOs>::get();
}
