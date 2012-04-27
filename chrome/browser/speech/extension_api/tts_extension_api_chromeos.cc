// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_extension_api_chromeos.h"

#include <list>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_controller.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_platform.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/speech_synthesizer_client.h"

using base::DoubleToString;

namespace {
const int kSpeechCheckDelayIntervalMs = 100;
};

// Very simple queue of utterances that can't be spoken because audio
// isn't initialized yet.
struct QueuedUtterance {
  int utterance_id;
  std::string utterance;
  std::string lang;
  UtteranceContinuousParameters params;
};

class ExtensionTtsPlatformImplChromeOs
    : public ExtensionTtsPlatformImpl {
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

  virtual bool IsSpeaking();

  virtual bool SendsEvent(TtsEventType event_type);

  // Get the single instance of this class.
  static ExtensionTtsPlatformImplChromeOs* GetInstance();

  // TTS won't begin until this is called.
  void Enable();

 private:
  ExtensionTtsPlatformImplChromeOs();
  virtual ~ExtensionTtsPlatformImplChromeOs() {}

  void PollUntilSpeechFinishes(int utterance_id);
  void ContinuePollingIfSpeechIsNotFinished(int utterance_id, bool result);

  void AppendSpeakOption(std::string key,
                         std::string value,
                         std::string* options);

  int utterance_id_;
  int utterance_length_;
  std::list<QueuedUtterance*> queued_utterances_;
  bool enabled_;
  base::WeakPtrFactory<ExtensionTtsPlatformImplChromeOs> weak_ptr_factory_;

  friend struct DefaultSingletonTraits<ExtensionTtsPlatformImplChromeOs>;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImplChromeOs);
};

// static
ExtensionTtsPlatformImpl* ExtensionTtsPlatformImpl::GetInstance() {
  return ExtensionTtsPlatformImplChromeOs::GetInstance();
}

ExtensionTtsPlatformImplChromeOs::ExtensionTtsPlatformImplChromeOs()
    : enabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

bool ExtensionTtsPlatformImplChromeOs::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const UtteranceContinuousParameters& params) {
  if (!enabled_) {
    QueuedUtterance *queued = new QueuedUtterance();
    queued->utterance_id = utterance_id;
    queued->utterance = utterance;
    queued->lang = lang;
    queued->params = params;
    queued_utterances_.push_back(queued);
    return true;
  }

  utterance_id_ = utterance_id;
  utterance_length_ = utterance.size();

  std::string options;

  if (!lang.empty()) {
    AppendSpeakOption(
        chromeos::SpeechSynthesizerClient::kSpeechPropertyLocale,
        lang,
        &options);
  }

  if (params.rate >= 0.0) {
    AppendSpeakOption(
        chromeos::SpeechSynthesizerClient::kSpeechPropertyRate,
        DoubleToString(params.rate),
        &options);
  }

  if (params.pitch >= 0.0) {
    // The TTS service allows a range of 0 to 2 for speech pitch.
    AppendSpeakOption(
        chromeos::SpeechSynthesizerClient::kSpeechPropertyPitch,
        DoubleToString(params.pitch),
        &options);
  }

  if (params.volume >= 0.0) {
    // The Chrome OS TTS service allows a range of 0 to 5 for speech volume,
    // but 5 clips, so map to a range of 0...4.
    AppendSpeakOption(
        chromeos::SpeechSynthesizerClient::kSpeechPropertyVolume,
        DoubleToString(params.volume * 4),
        &options);
  }

  chromeos::SpeechSynthesizerClient* speech_synthesizer_client =
      chromeos::DBusThreadManager::Get()->GetSpeechSynthesizerClient();

  speech_synthesizer_client->Speak(utterance, options);
  if (utterance_id_ >= 0) {
    ExtensionTtsController* controller = ExtensionTtsController::GetInstance();
    controller->OnTtsEvent(utterance_id_, TTS_EVENT_START, 0, std::string());
    PollUntilSpeechFinishes(utterance_id_);
  }

  return true;
}

bool ExtensionTtsPlatformImplChromeOs::StopSpeaking() {
  // If we haven't been enabled yet, clear the internal queue.
  if (!enabled_) {
    STLDeleteElements(&queued_utterances_);
    return true;
  }

  chromeos::DBusThreadManager::Get()->GetSpeechSynthesizerClient()->
      StopSpeaking();
  return true;
}

bool ExtensionTtsPlatformImplChromeOs::IsSpeaking() {
  // Defers to controller's utterance based detection of is speaking.
  return true;
}

bool ExtensionTtsPlatformImplChromeOs::SendsEvent(TtsEventType event_type) {
  return (event_type == TTS_EVENT_START ||
          event_type == TTS_EVENT_END ||
          event_type == TTS_EVENT_ERROR);
}

void ExtensionTtsPlatformImplChromeOs::Enable() {
  enabled_ = true;
  while (!queued_utterances_.empty()) {
    QueuedUtterance* queued = queued_utterances_.front();
    Speak(queued->utterance_id,
          queued->utterance,
          queued->lang,
          queued->params);
    delete queued;
    queued_utterances_.pop_front();
  }
}

void ExtensionTtsPlatformImplChromeOs::PollUntilSpeechFinishes(
    int utterance_id) {
  if (utterance_id != utterance_id_) {
    // This utterance must have been interrupted or cancelled.
    return;
  }
  chromeos::SpeechSynthesizerClient* speech_synthesizer_client =
      chromeos::DBusThreadManager::Get()->GetSpeechSynthesizerClient();
  speech_synthesizer_client->IsSpeaking(base::Bind(
      &ExtensionTtsPlatformImplChromeOs::ContinuePollingIfSpeechIsNotFinished,
      weak_ptr_factory_.GetWeakPtr(), utterance_id));
}

void ExtensionTtsPlatformImplChromeOs::ContinuePollingIfSpeechIsNotFinished(
    int utterance_id, bool is_speaking) {
  if (utterance_id != utterance_id_) {
    // This utterance must have been interrupted or cancelled.
    return;
  }
  if (!is_speaking) {
    ExtensionTtsController* controller = ExtensionTtsController::GetInstance();
    controller->OnTtsEvent(
        utterance_id_, TTS_EVENT_END, utterance_length_, std::string());
    return;
  }
  // Continue polling.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(
          &ExtensionTtsPlatformImplChromeOs::PollUntilSpeechFinishes,
          weak_ptr_factory_.GetWeakPtr(),
          utterance_id),
      base::TimeDelta::FromMilliseconds(kSpeechCheckDelayIntervalMs));
}

void ExtensionTtsPlatformImplChromeOs::AppendSpeakOption(
    std::string key,
    std::string value,
    std::string* options) {
  *options +=
      key +
      chromeos::SpeechSynthesizerClient::kSpeechPropertyEquals +
      value +
      chromeos::SpeechSynthesizerClient::kSpeechPropertyDelimiter;
}

// static
ExtensionTtsPlatformImplChromeOs*
ExtensionTtsPlatformImplChromeOs::GetInstance() {
  return Singleton<ExtensionTtsPlatformImplChromeOs>::get();
}

// global
void EnableChromeOsTts() {
  ExtensionTtsPlatformImplChromeOs::GetInstance()->Enable();
}
