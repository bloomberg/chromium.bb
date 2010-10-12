// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_tts_api_util.h"

// Abstract class that defines the native platform TTS interface.
class ExtensionTtsPlatformImpl {
 public:
  static ExtensionTtsPlatformImpl* GetInstance();

  // Speak the given utterance with the given parameters if possible,
  // and return true on success. Utterance will always be nonempty.
  // If the user does not specify the other values, language and gender
  // will be empty strings, and rate, pitch, and volume will be -1.0.
  virtual bool Speak(
      const std::string& utterance,
      const std::string& language,
      const std::string& gender,
      double rate,
      double pitch,
      double volume) = 0;

  // Stop speaking immediately and return true on success.
  virtual bool StopSpeaking() = 0;

  // Return true if the synthesis engine is currently speaking.
  virtual bool IsSpeaking() = 0;

  virtual std::string error() { return error_; }
  virtual void clear_error() { error_ = std::string(); }
  virtual void set_error(const std::string& error) { error_ = error; }

 protected:
  ExtensionTtsPlatformImpl() {}
  virtual ~ExtensionTtsPlatformImpl() {}

  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImpl);
};

//
// Extension API function definitions
//

class ExtensionTtsSpeakFunction : public SyncExtensionFunction {
  ~ExtensionTtsSpeakFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.tts.speak")
};

class ExtensionTtsStopSpeakingFunction : public SyncExtensionFunction {
  ~ExtensionTtsStopSpeakingFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.tts.stop")
};

class ExtensionTtsIsSpeakingFunction : public SyncExtensionFunction {
  ~ExtensionTtsIsSpeakingFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.tts.isSpeaking")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_
