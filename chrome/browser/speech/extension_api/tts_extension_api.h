// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_controller.h"

class ExtensionTtsSpeakFunction
    : public AsyncExtensionFunction {
 private:
  virtual ~ExtensionTtsSpeakFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tts.speak")
};

class ExtensionTtsStopSpeakingFunction : public SyncExtensionFunction {
 private:
  virtual ~ExtensionTtsStopSpeakingFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tts.stop")
};

class ExtensionTtsIsSpeakingFunction : public SyncExtensionFunction {
 private:
  virtual ~ExtensionTtsIsSpeakingFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tts.isSpeaking")
};

class ExtensionTtsGetVoicesFunction : public SyncExtensionFunction {
 private:
  virtual ~ExtensionTtsGetVoicesFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tts.getVoices")
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_
