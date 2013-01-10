// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

struct TtsVoice {
  TtsVoice();
  ~TtsVoice();

  std::string voice_name;
  std::string lang;
  std::string gender;
  std::set<std::string> event_types;

  static const std::vector<TtsVoice>* GetTtsVoices(const Extension* extension);
};

// Parses the "tts_engine" manifest key.
class TtsEngineManifestHandler : public ManifestHandler {
 public:
  TtsEngineManifestHandler();
  virtual ~TtsEngineManifestHandler();

  virtual bool Parse(const base::Value* value, Extension* extension,
                     string16* error) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
