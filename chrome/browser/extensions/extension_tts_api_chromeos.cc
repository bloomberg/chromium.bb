// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"
#include "chrome/browser/extensions/extension_tts_api_controller.h"
#include "chrome/browser/extensions/extension_tts_api_platform.h"

using base::DoubleToString;

namespace {
const char kCrosLibraryNotLoadedError[] = "Cros shared library not loaded.";
const int kSpeechCheckDelayIntervalMs = 100;
};

class ExtensionTtsPlatformImplChromeOs : public ExtensionTtsPlatformImpl {
 public:
  virtual bool PlatformImplAvailable() {
    return true;
  }

  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params);

  virtual bool StopSpeaking();

  virtual bool SendsEvent(TtsEventType event_type);

  // Get the single instance of this class.
  static ExtensionTtsPlatformImplChromeOs* GetInstance();

 private:
  ExtensionTtsPlatformImplChromeOs()
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}
  virtual ~ExtensionTtsPlatformImplChromeOs() {}

  void PollUntilSpeechFinishes(int utterance_id);

  void AppendSpeakOption(std::string key,
                         std::string value,
                         std::string* options);

  int utterance_id_;
  int utterance_length_;
  ScopedRunnableMethodFactory<ExtensionTtsPlatformImplChromeOs> method_factory_;

  friend struct DefaultSingletonTraits<ExtensionTtsPlatformImplChromeOs>;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImplChromeOs);
};

// static
ExtensionTtsPlatformImpl* ExtensionTtsPlatformImpl::GetInstance() {
  return ExtensionTtsPlatformImplChromeOs::GetInstance();
}

bool ExtensionTtsPlatformImplChromeOs::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const UtteranceContinuousParameters& params) {
  chromeos::CrosLibrary* cros_library = chromeos::CrosLibrary::Get();
  if (!cros_library->EnsureLoaded()) {
    set_error(kCrosLibraryNotLoadedError);
    return false;
  }

  utterance_id_ = utterance_id;
  utterance_length_ = utterance.size();

  std::string options;

  if (!lang.empty()) {
    AppendSpeakOption(
        chromeos::SpeechSynthesisLibrary::kSpeechPropertyLocale,
        lang,
        &options);
  }

  if (params.rate >= 0.0) {
    AppendSpeakOption(
        chromeos::SpeechSynthesisLibrary::kSpeechPropertyRate,
        DoubleToString(params.rate),
        &options);
  }

  if (params.pitch >= 0.0) {
    // The TTS service allows a range of 0 to 2 for speech pitch.
    AppendSpeakOption(
        chromeos::SpeechSynthesisLibrary::kSpeechPropertyPitch,
        DoubleToString(params.pitch),
        &options);
  }

  if (params.volume >= 0.0) {
    // The Chrome OS TTS service allows a range of 0 to 5 for speech volume,
    // but 5 clips, so map to a range of 0...4.
    AppendSpeakOption(
        chromeos::SpeechSynthesisLibrary::kSpeechPropertyVolume,
        DoubleToString(params.volume * 4),
        &options);
  }

  if (!options.empty()) {
    cros_library->GetSpeechSynthesisLibrary()->SetSpeakProperties(
        options.c_str());
  }

  bool result =
      cros_library->GetSpeechSynthesisLibrary()->Speak(utterance.c_str());

  if (result) {
    ExtensionTtsController* controller = ExtensionTtsController::GetInstance();
    controller->OnTtsEvent(utterance_id_, TTS_EVENT_START, 0, std::string());
    PollUntilSpeechFinishes(utterance_id_);
  }

  return result;
}

bool ExtensionTtsPlatformImplChromeOs::StopSpeaking() {
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    return chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
        StopSpeaking();
  }

  set_error(kCrosLibraryNotLoadedError);
  return false;
}

bool ExtensionTtsPlatformImplChromeOs::SendsEvent(TtsEventType event_type) {
  return (event_type == TTS_EVENT_START ||
          event_type == TTS_EVENT_END ||
          event_type == TTS_EVENT_ERROR);
}

void ExtensionTtsPlatformImplChromeOs::PollUntilSpeechFinishes(
    int utterance_id) {
  if (utterance_id != utterance_id_) {
    // This utterance must have been interrupted or cancelled.
    return;
  }

  chromeos::CrosLibrary* cros_library = chromeos::CrosLibrary::Get();
  ExtensionTtsController* controller = ExtensionTtsController::GetInstance();

  if (!cros_library->EnsureLoaded()) {
    controller->OnTtsEvent(
        utterance_id_, TTS_EVENT_ERROR, 0, kCrosLibraryNotLoadedError);
    return;
  }

  if (!cros_library->GetSpeechSynthesisLibrary()->IsSpeaking()) {
    controller->OnTtsEvent(
        utterance_id_, TTS_EVENT_END, utterance_length_, std::string());
    return;
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, method_factory_.NewRunnableMethod(
          &ExtensionTtsPlatformImplChromeOs::PollUntilSpeechFinishes,
          utterance_id),
      kSpeechCheckDelayIntervalMs);
}

void ExtensionTtsPlatformImplChromeOs::AppendSpeakOption(
    std::string key,
    std::string value,
    std::string* options) {
  *options +=
      key +
      chromeos::SpeechSynthesisLibrary::kSpeechPropertyEquals +
      value +
      chromeos::SpeechSynthesisLibrary::kSpeechPropertyDelimiter;
}

// static
ExtensionTtsPlatformImplChromeOs*
ExtensionTtsPlatformImplChromeOs::GetInstance() {
  return Singleton<ExtensionTtsPlatformImplChromeOs>::get();
}
