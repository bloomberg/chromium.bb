// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_

#include <string>

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_controller.h"

class Profile;

namespace extensions {

class TtsSpeakFunction
    : public AsyncExtensionFunction {
 private:
  virtual ~TtsSpeakFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tts.speak", TTS_SPEAK)
};

class TtsStopSpeakingFunction : public SyncExtensionFunction {
 private:
  virtual ~TtsStopSpeakingFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tts.stop", TTS_STOP)
};

class TtsIsSpeakingFunction : public SyncExtensionFunction {
 private:
  virtual ~TtsIsSpeakingFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tts.isSpeaking", TTS_ISSPEAKING)
};

class TtsGetVoicesFunction : public SyncExtensionFunction {
 private:
  virtual ~TtsGetVoicesFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("tts.getVoices", TTS_GETVOICES)
};

class TtsAPI : public ProfileKeyedAPI {
 public:
  explicit TtsAPI(Profile* profile);
  virtual ~TtsAPI();

  // Convenience method to get the TtsAPI for a profile.
  static TtsAPI* Get(Profile* profile);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<TtsAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<TtsAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "TtsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_
