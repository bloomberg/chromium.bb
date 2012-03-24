// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_platform.h"

namespace {
const char kNotSupportedError[] =
    "Native speech synthesis not supported on this platform.";
};

class ExtensionTtsPlatformImplLinux : public ExtensionTtsPlatformImpl {
 public:
  virtual bool PlatformImplAvailable() {
    return false;
  }

  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params) {
    error_ = kNotSupportedError;
    return false;
  }

  virtual bool StopSpeaking() {
    error_ = kNotSupportedError;
    return false;
  }

  virtual bool IsSpeaking() {
    error_ = kNotSupportedError;
    return false;
  }

  virtual bool SendsEvent(TtsEventType event_type) {
    return false;
  }

  // Get the single instance of this class.
  static ExtensionTtsPlatformImplLinux* GetInstance() {
    return Singleton<ExtensionTtsPlatformImplLinux>::get();
  }

 private:
  ExtensionTtsPlatformImplLinux() {}
  virtual ~ExtensionTtsPlatformImplLinux() {}

  friend struct DefaultSingletonTraits<ExtensionTtsPlatformImplLinux>;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImplLinux);
};

// static
ExtensionTtsPlatformImpl* ExtensionTtsPlatformImpl::GetInstance() {
  return ExtensionTtsPlatformImplLinux::GetInstance();
}
