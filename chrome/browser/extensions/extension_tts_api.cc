// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api.h"

#include <string>

#include "base/values.h"
#include "base/string_number_conversions.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"

namespace util = extension_tts_api_util;

using base::DoubleToString;

namespace {
  const char kCrosLibraryNotLoadedError[] =
      "Cros shared library not loaded.";
};

bool ExtensionTtsSpeakFunction::RunImpl() {
  std::string utterance;
  std::string options = "";
  DictionaryValue* speak_options = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance));
  if (args_->GetDictionary(1, &speak_options)) {
    std::string str_value;
    double real_value;
    if (speak_options->HasKey(util::kLanguageNameKey) &&
      speak_options->GetString(util::kLanguageNameKey, &str_value)) {
        util::AppendSpeakOption(
            std::string(util::kNameKey), str_value, &options);
    }
    if (speak_options->HasKey(util::kGenderKey) &&
      speak_options->GetString(util::kGenderKey, &str_value)) {
        util::AppendSpeakOption(
            std::string(util::kGenderKey), str_value, &options);
    }
    if (util::ReadNumberByKey(speak_options, util::kRateKey, &real_value)) {
      // The TTS service allows a range of 0 to 5 for speech rate.
      util::AppendSpeakOption(std::string(util::kRateKey),
          DoubleToString(real_value * 5), &options);
    }
    if (util::ReadNumberByKey(speak_options, util::kPitchKey, &real_value)) {
      // The TTS service allows a range of 0 to 2 for speech pitch.
      util::AppendSpeakOption(std::string(util::kPitchKey),
          DoubleToString(real_value * 2), &options);
    }
    if (util::ReadNumberByKey(speak_options, util::kVolumeKey, &real_value)) {
      // The TTS service allows a range of 0 to 5 for speech volume.
      util::AppendSpeakOption(std::string(util::kVolumeKey),
          DoubleToString(real_value * 5), &options);
    }
  }
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    if (!options.empty()) {
      chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
          SetSpeakProperties(options.c_str());
    }
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
