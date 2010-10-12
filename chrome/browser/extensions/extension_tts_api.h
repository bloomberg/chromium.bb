// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_tts_api_util.h"

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
