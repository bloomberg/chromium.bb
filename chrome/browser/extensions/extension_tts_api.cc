// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api.h"

#include <string>

#include "base/values.h"
#include "base/string_number_conversions.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"

using base::DoubleToString;

const char kNameKey[] = "name";
const char kLanguageNameKey[] = "languageName";
const char kGenderKey[] = "gender";
const char kRateKey[] = "rate";
const char kPitchKey[] = "pitch";
const char kVolumeKey[] = "volume";
const char kEqualStr[] = "=";
const char kDelimiter[] = ";";

namespace {
  const char kCrosLibraryNotLoadedError[] =
      "Cros shared library not loaded.";

  bool ReadNumberByKey(DictionaryValue* dict, const char* key,
      double* ret_value) {
    Value* value;
    dict->Get(key, &value);
    if (value->IsType(Value::TYPE_INTEGER)) {
      int int_value;
      if (!dict->GetInteger(key, &int_value))
        return false;
      *ret_value = int_value;
    } else if (value->IsType(Value::TYPE_REAL)) {
      if (!dict->GetReal(key, ret_value))
        return false;
    } else {
      return false;
    }
    return true;
  }

  void AppendSpeakOption(std::string key, std::string value,
      std::string* options) {
    *options += key + kEqualStr + value + kDelimiter;
  }
};

bool ExtensionTtsSpeakFunction::RunImpl() {
  std::string utterance;
  std::string options = "";
  DictionaryValue* speak_options = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance));
  if (args_->GetDictionary(1, &speak_options)) {
    std::string str_value;
    double real_value;
    if (speak_options->HasKey(kLanguageNameKey) &&
        speak_options->GetString(kLanguageNameKey, &str_value)) {
      AppendSpeakOption(std::string(kNameKey), str_value, &options);
    }
    if (speak_options->HasKey(kGenderKey) &&
        speak_options->GetString(kGenderKey, &str_value)) {
      AppendSpeakOption(std::string(kGenderKey), str_value, &options);
    }
    if (ReadNumberByKey(speak_options, kRateKey, &real_value))
      // The TTS service allows a range of 0 to 5 for speech rate.
      AppendSpeakOption(std::string(kRateKey),
                        DoubleToString(real_value * 5), &options);
    if (ReadNumberByKey(speak_options, kPitchKey, &real_value))
      // The TTS service allows a range of 0 to 2 for speech pitch.
      AppendSpeakOption(std::string(kPitchKey),
                        DoubleToString(real_value * 2), &options);
    if (ReadNumberByKey(speak_options, kVolumeKey, &real_value))
      // The TTS service allows a range of 0 to 5 for speech volume.
      AppendSpeakOption(std::string(kVolumeKey),
                        DoubleToString(real_value * 5), &options);
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
