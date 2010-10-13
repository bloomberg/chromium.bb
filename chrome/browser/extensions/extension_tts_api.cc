// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api.h"

#include <string>

#include "base/float_util.h"
#include "base/values.h"

namespace util = extension_tts_api_util;

namespace {
  const char kCrosLibraryNotLoadedError[] =
      "Cros shared library not loaded.";
};

bool ExtensionTtsSpeakFunction::RunImpl() {
  std::string utterance;
  std::string language;
  std::string gender;
  double rate = -1.0;
  double pitch = -1.0;
  double volume = -1.0;

  DictionaryValue* speak_options = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance));

  if (args_->GetDictionary(1, &speak_options)) {
    if (speak_options->HasKey(util::kLanguageNameKey)) {
      speak_options->GetString(util::kLanguageNameKey, &language);
    }

    if (speak_options->HasKey(util::kGenderKey)) {
      speak_options->GetString(util::kGenderKey, &gender);
    }

    if (util::ReadNumberByKey(speak_options, util::kRateKey, &rate)) {
      if (!base::IsFinite(rate) || rate < 0.0 || rate > 1.0) {
        rate = -1.0;
      }
    }

    if (util::ReadNumberByKey(speak_options, util::kPitchKey, &pitch)) {
      if (!base::IsFinite(pitch) || pitch < 0.0 || pitch > 1.0) {
        pitch = -1.0;
      }
    }

    if (util::ReadNumberByKey(speak_options, util::kVolumeKey, &volume)) {
      if (!base::IsFinite(volume) || volume < 0.0 || volume > 1.0) {
        volume = -1.0;
      }
    }
  }

  ExtensionTtsPlatformImpl* impl = ExtensionTtsPlatformImpl::GetInstance();
  impl->clear_error();
  return impl->Speak(utterance, language, gender, rate, pitch, volume);
}

bool ExtensionTtsStopSpeakingFunction::RunImpl() {
  ExtensionTtsPlatformImpl* impl = ExtensionTtsPlatformImpl::GetInstance();
  impl->clear_error();
  return impl->StopSpeaking();
}

bool ExtensionTtsIsSpeakingFunction::RunImpl() {
  ExtensionTtsPlatformImpl* impl = ExtensionTtsPlatformImpl::GetInstance();
  impl->clear_error();
  result_.reset(Value::CreateBooleanValue(impl->IsSpeaking()));
  return true;
}
