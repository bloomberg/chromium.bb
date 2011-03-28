// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api.h"

#include "base/memory/singleton.h"

namespace util = extension_tts_api_util;

namespace {
const char kNotSupportedError[] =
    "Native speech synthesis not supported on this platform.";
};

class ExtensionTtsPlatformImplLinux : public ExtensionTtsPlatformImpl {
 public:
  virtual bool Speak(
      const std::string& utterance,
      const std::string& language,
      const std::string& gender,
      double rate,
      double pitch,
      double volume) {
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
