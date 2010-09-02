// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_tts_api.h"

#include <atlbase.h>
#include <atlcom.h>
#include <sapi.h>

#include "base/scoped_comptr_win.h"
#include "base/singleton.h"
#include "base/values.h"

class SpeechSynthesizerWrapper {
 public:
  SpeechSynthesizerWrapper() : speech_synthesizer_(NULL),
                               paused_(false),
                               permanent_failure_(false) {
    InitializeSpeechSynthesizer();
  }

  bool InitializeSpeechSynthesizer() {
    if (!SUCCEEDED(CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_SERVER,
        IID_ISpVoice, reinterpret_cast<void**>(&speech_synthesizer_)))) {
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
