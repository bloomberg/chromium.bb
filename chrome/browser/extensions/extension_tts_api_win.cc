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
#include "base/utf_string_conversions.h"
#include "base/values.h"

namespace util = extension_tts_api_util;

class ExtensionTtsPlatformImplWin : public ExtensionTtsPlatformImpl {
 public:
  virtual bool Speak(
      const std::string& utterance,
      const std::string& language,
      const std::string& gender,
      double rate,
      double pitch,
      double volume);

  virtual bool StopSpeaking();

  virtual bool IsSpeaking();

  // Get the single instance of this class.
  static ExtensionTtsPlatformImplWin* GetInstance();

 private:
  ExtensionTtsPlatformImplWin();
  virtual ~ExtensionTtsPlatformImplWin() {}

  ScopedComPtr<ISpVoice> speech_synthesizer_;
  bool paused_;

  friend struct DefaultSingletonTraits<ExtensionTtsPlatformImplWin>;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImplWin);
};

// static
ExtensionTtsPlatformImpl* ExtensionTtsPlatformImpl::GetInstance() {
  return ExtensionTtsPlatformImplWin::GetInstance();
}

bool ExtensionTtsPlatformImplWin::Speak(
    const std::string& src_utterance,
    const std::string& language,
    const std::string& gender,
    double rate,
    double pitch,
    double volume) {
  std::wstring utterance = UTF8ToUTF16(src_utterance);

  if (!speech_synthesizer_)
    return false;

  // Speech API equivalents for kGenderKey and kLanguageNameKey do not
  // exist and thus are not supported.

  if (rate >= 0.0) {
    // The TTS api allows a range of -10 to 10 for speech rate.
    speech_synthesizer_->SetRate(static_cast<int32>(rate * 20 - 10));
  }

  if (pitch >= 0.0) {
    // The TTS api allows a range of -10 to 10 for speech pitch.
    // TODO(dtseng): cleanup if we ever use any other properties that
    // require xml.
    std::wstring pitch_value =
        base::IntToString16(static_cast<int>(pitch * 20 - 10));
    utterance = L"<pitch absmiddle=\"" + pitch_value + L"\">" +
        utterance + L"</pitch>";
  }

  if (volume >= 0.0) {
    // The TTS api allows a range of 0 to 100 for speech volume.
    speech_synthesizer_->SetVolume(static_cast<uint16>(volume * 100));
  }

  if (paused_)
    speech_synthesizer_->Resume();
  speech_synthesizer_->Speak(
      utterance.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, NULL);

  return true;
}

bool ExtensionTtsPlatformImplWin::StopSpeaking() {
  if (speech_synthesizer_ && !paused_) {
    speech_synthesizer_->Pause();
    paused_ = true;
  }
  return true;
}

bool ExtensionTtsPlatformImplWin::IsSpeaking() {
  if (speech_synthesizer_ && !paused_) {
    SPVOICESTATUS status;
    HRESULT result = speech_synthesizer_->GetStatus(&status, NULL);
    if (result == S_OK && status.dwRunningState == SPRS_IS_SPEAKING)
      return true;
  }
  return false;
}

ExtensionTtsPlatformImplWin::ExtensionTtsPlatformImplWin()
  : speech_synthesizer_(NULL),
    paused_(false) {
  CoCreateInstance(
      CLSID_SpVoice,
      NULL,
      CLSCTX_SERVER,
      IID_ISpVoice,
      reinterpret_cast<void**>(&speech_synthesizer_));
}

// static
ExtensionTtsPlatformImplWin* ExtensionTtsPlatformImplWin::GetInstance() {
  return Singleton<ExtensionTtsPlatformImplWin>::get();
}
