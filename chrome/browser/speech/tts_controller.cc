// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_controller.h"

#include <string>
#include <vector>

#include "base/float_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/extension.h"

namespace constants = tts_extension_api_constants;

namespace {
// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;
}  // namespace


bool IsFinalTtsEventType(TtsEventType event_type) {
  return (event_type == TTS_EVENT_END ||
          event_type == TTS_EVENT_INTERRUPTED ||
          event_type == TTS_EVENT_CANCELLED ||
          event_type == TTS_EVENT_ERROR);
}


//
// UtteranceContinuousParameters
//


UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(-1),
      pitch(-1),
      volume(-1) {}


//
// VoiceData
//


VoiceData::VoiceData() {}

VoiceData::~VoiceData() {}


//
// Utterance
//

// static
int Utterance::next_utterance_id_ = 0;

Utterance::Utterance(Profile* profile)
    : profile_(profile),
      id_(next_utterance_id_++),
      src_id_(-1),
      event_delegate_(NULL),
      can_enqueue_(false),
      char_index_(0),
      finished_(false) {
  options_.reset(new DictionaryValue());
}

Utterance::~Utterance() {
  DCHECK(finished_);
}

void Utterance::OnTtsEvent(TtsEventType event_type,
                           int char_index,
                           const std::string& error_message) {
  if (char_index >= 0)
    char_index_ = char_index;
  if (IsFinalTtsEventType(event_type))
    finished_ = true;

  if (event_delegate_)
    event_delegate_->OnTtsEvent(this, event_type, char_index, error_message);
  if (finished_)
    event_delegate_ = NULL;
}

void Utterance::Finish() {
  finished_ = true;
}

void Utterance::set_options(const Value* options) {
  options_.reset(options->DeepCopy());
}

//
// TtsController
//

// static
TtsController* TtsController::GetInstance() {
  return Singleton<TtsController>::get();
}

TtsController::TtsController()
    : current_utterance_(NULL),
      platform_impl_(NULL) {
}

TtsController::~TtsController() {
  if (current_utterance_) {
    current_utterance_->Finish();
    delete current_utterance_;
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.
}

void TtsController::SpeakOrEnqueue(Utterance* utterance) {
  if (IsSpeaking() && utterance->can_enqueue()) {
    utterance_queue_.push(utterance);
  } else {
    Stop();
    SpeakNow(utterance);
  }
}

void TtsController::SpeakNow(Utterance* utterance) {
  const extensions::Extension* extension;
  size_t voice_index;
  if (GetMatchingExtensionVoice(utterance, &extension, &voice_index)) {
    current_utterance_ = utterance;
    utterance->set_extension_id(extension->id());

    ExtensionTtsEngineSpeak(utterance, extension, voice_index);

    const std::vector<extensions::TtsVoice>* tts_voices =
        extensions::TtsVoice::GetTtsVoices(extension);
    std::set<std::string> event_types;
    if (tts_voices)
      event_types = tts_voices->at(voice_index).event_types;
    bool sends_end_event =
        (event_types.find(constants::kEventTypeEnd) != event_types.end());
    if (!sends_end_event) {
      utterance->Finish();
      delete utterance;
      current_utterance_ = NULL;
      SpeakNextUtterance();
    }
    return;
  }

  GetPlatformImpl()->clear_error();
  bool success = GetPlatformImpl()->Speak(
      utterance->id(),
      utterance->text(),
      utterance->lang(),
      utterance->continuous_parameters());

  if (!success &&
      GetPlatformImpl()->LoadBuiltInTtsExtension(utterance->profile())) {
    utterance_queue_.push(utterance);
    return;
  }

  if (!success) {
    utterance->OnTtsEvent(TTS_EVENT_ERROR, kInvalidCharIndex,
                          GetPlatformImpl()->error());
    delete utterance;
    return;
  }
  current_utterance_ = utterance;
}

void TtsController::Stop() {
  if (current_utterance_ && !current_utterance_->extension_id().empty()) {
    ExtensionTtsEngineStop(current_utterance_);
  } else {
    GetPlatformImpl()->clear_error();
    GetPlatformImpl()->StopSpeaking();
  }

  if (current_utterance_)
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                   std::string());
  FinishCurrentUtterance();
  ClearUtteranceQueue(true);  // Send events.
}

void TtsController::OnTtsEvent(int utterance_id,
                                        TtsEventType event_type,
                                        int char_index,
                                        const std::string& error_message) {
  // We may sometimes receive completion callbacks "late", after we've
  // already finished the utterance (for example because another utterance
  // interrupted or we got a call to Stop). This is normal and we can
  // safely just ignore these events.
  if (!current_utterance_ || utterance_id != current_utterance_->id())
    return;

  current_utterance_->OnTtsEvent(event_type, char_index, error_message);
  if (current_utterance_->finished()) {
    FinishCurrentUtterance();
    SpeakNextUtterance();
  }
}

void TtsController::GetVoices(Profile* profile,
                              std::vector<VoiceData>* out_voices) {
  TtsPlatformImpl* platform_impl = GetPlatformImpl();
  if (platform_impl && platform_impl->PlatformImplAvailable()) {
    out_voices->push_back(VoiceData());
    VoiceData& voice = out_voices->back();
    voice.name = constants::kNativeVoiceName;
    voice.gender = platform_impl->gender();

    // All platforms must send end events, and cancelled and interrupted
    // events are generated from the controller.
    DCHECK(platform_impl->SendsEvent(TTS_EVENT_END));
    voice.events.push_back(constants::kEventTypeEnd);
    voice.events.push_back(constants::kEventTypeCancelled);
    voice.events.push_back(constants::kEventTypeInterrupted);

    if (platform_impl->SendsEvent(TTS_EVENT_START))
      voice.events.push_back(constants::kEventTypeStart);
    if (platform_impl->SendsEvent(TTS_EVENT_WORD))
      voice.events.push_back(constants::kEventTypeWord);
    if (platform_impl->SendsEvent(TTS_EVENT_SENTENCE))
      voice.events.push_back(constants::kEventTypeSentence);
    if (platform_impl->SendsEvent(TTS_EVENT_MARKER))
      voice.events.push_back(constants::kEventTypeMarker);
    if (platform_impl->SendsEvent(TTS_EVENT_ERROR))
      voice.events.push_back(constants::kEventTypeError);
  }

  GetExtensionVoices(profile, out_voices);
}

bool TtsController::IsSpeaking() {
  return current_utterance_ != NULL || GetPlatformImpl()->IsSpeaking();
}

void TtsController::FinishCurrentUtterance() {
  if (current_utterance_) {
    if (!current_utterance_->finished())
      current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                     std::string());
    delete current_utterance_;
    current_utterance_ = NULL;
  }
}

void TtsController::SpeakNextUtterance() {
  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  while (!utterance_queue_.empty() && !current_utterance_) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    SpeakNow(utterance);
  }
}

void TtsController::RetrySpeakingQueuedUtterances() {
  if (current_utterance_ == NULL && !utterance_queue_.empty())
    SpeakNextUtterance();
}

void TtsController::ClearUtteranceQueue(bool send_events) {
  while (!utterance_queue_.empty()) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    if (send_events)
      utterance->OnTtsEvent(TTS_EVENT_CANCELLED, kInvalidCharIndex,
                            std::string());
    else
      utterance->Finish();
    delete utterance;
  }
}

void TtsController::SetPlatformImpl(
    TtsPlatformImpl* platform_impl) {
  platform_impl_ = platform_impl;
}

int TtsController::QueueSize() {
  return static_cast<int>(utterance_queue_.size());
}

TtsPlatformImpl* TtsController::GetPlatformImpl() {
  if (!platform_impl_)
    platform_impl_ = TtsPlatformImpl::GetInstance();
  return platform_impl_;
}
