// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_controller.h"

#include <string>
#include <vector>

#include "base/float_util.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
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

namespace events {
const char kOnEvent[] = "tts.onEvent";
};  // namespace events

std::string TtsEventTypeToString(TtsEventType event_type) {
  switch (event_type) {
    case TTS_EVENT_START:
      return constants::kEventTypeStart;
    case TTS_EVENT_END:
      return constants::kEventTypeEnd;
    case TTS_EVENT_WORD:
      return constants::kEventTypeWord;
    case TTS_EVENT_SENTENCE:
      return constants::kEventTypeSentence;
    case TTS_EVENT_MARKER:
      return constants::kEventTypeMarker;
    case TTS_EVENT_INTERRUPTED:
      return constants::kEventTypeInterrupted;
    case TTS_EVENT_CANCELLED:
      return constants::kEventTypeCancelled;
    case TTS_EVENT_ERROR:
      return constants::kEventTypeError;
    default:
      NOTREACHED();
      return std::string();
  }
}

}  // namespace

//
// UtteranceContinuousParameters
//


UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(-1),
      pitch(-1),
      volume(-1) {}


//
// Utterance
//

// static
int Utterance::next_utterance_id_ = 0;

Utterance::Utterance(Profile* profile)
    : profile_(profile),
      id_(next_utterance_id_++),
      src_id_(-1),
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
  std::string event_type_string = TtsEventTypeToString(event_type);
  if (char_index >= 0)
    char_index_ = char_index;
  if (event_type == TTS_EVENT_END ||
      event_type == TTS_EVENT_INTERRUPTED ||
      event_type == TTS_EVENT_CANCELLED ||
      event_type == TTS_EVENT_ERROR) {
    finished_ = true;
  }
  if (desired_event_types_.size() > 0 &&
      desired_event_types_.find(event_type_string) ==
      desired_event_types_.end()) {
    return;
  }

  if (src_id_ < 0)
    return;

  DictionaryValue* details = new DictionaryValue();
  if (char_index != kInvalidCharIndex)
    details->SetInteger(constants::kCharIndexKey, char_index);
  details->SetString(constants::kEventTypeKey, event_type_string);
  if (event_type == TTS_EVENT_ERROR) {
    details->SetString(constants::kErrorMessageKey, error_message);
  }
  details->SetInteger(constants::kSrcIdKey, src_id_);
  details->SetBoolean(constants::kIsFinalEventKey, finished_);

  scoped_ptr<ListValue> arguments(new ListValue());
  arguments->Set(0, details);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      events::kOnEvent, arguments.Pass()));
  event->restrict_to_profile = profile_;
  event->event_url = src_url_;
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(src_extension_id_, event.Pass());
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

ListValue* TtsController::GetVoices(Profile* profile) {
  ListValue* result_voices = new ListValue();
  TtsPlatformImpl* platform_impl = GetPlatformImpl();
  if (platform_impl && platform_impl->PlatformImplAvailable()) {
    DictionaryValue* result_voice = new DictionaryValue();
    result_voice->SetString(
        constants::kVoiceNameKey, constants::kNativeVoiceName);
    if (!platform_impl->gender().empty())
      result_voice->SetString(constants::kGenderKey, platform_impl->gender());
    ListValue* event_types = new ListValue();

    // All platforms must send end events, and cancelled and interrupted
    // events are generated from the controller.
    DCHECK(platform_impl->SendsEvent(TTS_EVENT_END));
    event_types->Append(Value::CreateStringValue(constants::kEventTypeEnd));
    event_types->Append(Value::CreateStringValue(
        constants::kEventTypeCancelled));
    event_types->Append(Value::CreateStringValue(
        constants::kEventTypeInterrupted));

    if (platform_impl->SendsEvent(TTS_EVENT_START))
      event_types->Append(Value::CreateStringValue(constants::kEventTypeStart));
    if (platform_impl->SendsEvent(TTS_EVENT_WORD))
      event_types->Append(Value::CreateStringValue(constants::kEventTypeWord));
    if (platform_impl->SendsEvent(TTS_EVENT_SENTENCE))
      event_types->Append(Value::CreateStringValue(
          constants::kEventTypeSentence));
    if (platform_impl->SendsEvent(TTS_EVENT_MARKER))
      event_types->Append(Value::CreateStringValue(
          constants::kEventTypeMarker));
    if (platform_impl->SendsEvent(TTS_EVENT_ERROR))
      event_types->Append(Value::CreateStringValue(
          constants::kEventTypeError));
    result_voice->Set(constants::kEventTypesKey, event_types);
    result_voices->Append(result_voice);
  }

  GetExtensionVoices(profile, result_voices);

  return result_voices;
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
