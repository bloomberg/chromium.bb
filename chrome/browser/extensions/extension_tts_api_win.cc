// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api.h"

#include <atlbase.h>
#include <atlcom.h>
#include <sapi.h>

#include "base/scoped_comptr_win.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/values.h"

namespace util = extension_tts_api_util;

class SpeechSynthesizerWrapper {
 public:
  SpeechSynthesizerWrapper() : speech_synthesizer_(NULL),
                               paused_(false),
                               permanent_failure_(false) {
    InitializeSpeechSynthesizer();
  }

  bool InitializeSpeechSynthesizer() {
    if (!SUCCEEDED(CoCreateInstance(CLSID_SpVoice,
                                    NULL,
                                    CLSCTX_SERVER,
                                    IID_ISpVoice,
                                    reinterpret_cast<void**>(
                                        &speech_synthesizer_)))) {
      permanent_failure_ = true;
      return false;
    }

    if (paused_)
      speech_synthesizer_->Resume();
    return true;
  }

  ScopedComPtr<ISpVoice> speech_synthesizer() {
    return speech_synthesizer_;
  }

  bool paused() {
    return paused_;
  }

  void paused(bool state) {
    paused_ = state;
  }

 private:
  ScopedComPtr<ISpVoice> speech_synthesizer_;
  bool paused_;
  // Indicates an error retrieving the SAPI COM interface.
  bool permanent_failure_;
};

typedef Singleton<SpeechSynthesizerWrapper> SpeechSynthesizerSingleton;

bool ExtensionTtsSpeakFunction::RunImpl() {
  ScopedComPtr<ISpVoice> speech_synthesizer =
      SpeechSynthesizerSingleton::get()->speech_synthesizer();
  if (speech_synthesizer) {
    std::wstring utterance;
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance));

    std::string options = "";
    DictionaryValue* speak_options = NULL;

    // Parse speech properties.
    if (args_->GetDictionary(1, &speak_options)) {
      std::string str_value;
      double real_value;
      // Speech API equivalents for kGenderKey and kLanguageNameKey do not
      // exist and thus are not supported.
      if (util::ReadNumberByKey(speak_options, util::kRateKey, &real_value)) {
        // The TTS api allows a range of -10 to 10 for speech rate.
        speech_synthesizer->SetRate(static_cast<int32>(real_value*20 - 10));
      }
      if (util::ReadNumberByKey(speak_options, util::kPitchKey, &real_value)) {
        // The TTS api allows a range of -10 to 10 for speech pitch.
        // TODO(dtseng): cleanup if we ever
        // use any other properties that require xml.
        std::wstring pitch_value =
            base::IntToString16(static_cast<int>(real_value*20 - 10));
        utterance = L"<pitch absmiddle=\"" + pitch_value + L"\">" +
            utterance + L"</pitch>";
      }
      if (util::ReadNumberByKey(
          speak_options, util::kVolumeKey, &real_value)) {
        // The TTS api allows a range of 0 to 100 for speech volume.
        speech_synthesizer->SetVolume(static_cast<uint16>(real_value * 100));
      }
    }

    if (SpeechSynthesizerSingleton::get()->paused())
      speech_synthesizer->Resume();
    speech_synthesizer->Speak(
        utterance.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, NULL);
    return true;
  }

  return false;
}

bool ExtensionTtsStopSpeakingFunction::RunImpl() {
  // We need to keep track of the paused state since SAPI doesn't have a stop
  // method.
  ScopedComPtr<ISpVoice> speech_synthesizer =
      SpeechSynthesizerSingleton::get()->speech_synthesizer();
  if (speech_synthesizer && !SpeechSynthesizerSingleton::get()->paused()) {
    speech_synthesizer->Pause();
    SpeechSynthesizerSingleton::get()->paused(true);
  }
  return true;
}

bool ExtensionTtsIsSpeakingFunction::RunImpl() {
  return false;
}
