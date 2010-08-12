// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api.h"

#include <string>

#include "base/values.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"

namespace {
  const char kCrosLibraryNotLoadedError[] =
      "Cros shared library not loaded.";
};

bool ExtensionTtsSpeakFunction::RunImpl() {
  std::string utterance;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance));
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    bool ret = chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
        Speak(utterance.c_str());
    result_.reset();
    return ret;
  }
  error_ = kCrosLibraryNotLoadedError;
  return false;
}

bool ExtensionTtsStopSpeakingFunction::RunImpl() {
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    return chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
        StopSpeaking();
  }
  error_ = kCrosLibraryNotLoadedError;
  return false;
}

bool ExtensionTtsIsSpeakingFunction::RunImpl() {
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    result_.reset(Value::CreateBooleanValue(
        chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
        IsSpeaking()));
    return true;
  }
  error_ = kCrosLibraryNotLoadedError;
  return false;
}
