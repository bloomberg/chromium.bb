// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api_controller.h"

#include <string>
#include <vector>

#include "base/float_util.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tts_api.h"
#include "chrome/browser/extensions/extension_tts_api_constants.h"
#include "chrome/browser/extensions/extension_tts_api_platform.h"
#include "chrome/browser/extensions/extension_tts_engine_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"

namespace constants = extension_tts_api_constants;

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

  ListValue args;
  DictionaryValue* event = new DictionaryValue();
  event->SetInteger(constants::kCharIndexKey, char_index);
  event->SetString(constants::kEventTypeKey, event_type_string);
  if (event_type == TTS_EVENT_ERROR) {
    event->SetString(constants::kErrorMessageKey, error_message);
  }
  event->SetInteger(constants::kSrcIdKey, src_id_);
  event->SetBoolean(constants::kIsFinalEventKey, finished_);
  args.Set(0, event);
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      src_extension_id_,
      events::kOnEvent,
      json_args,
      profile_,
      src_url_);
}

void Utterance::Finish() {
  finished_ = true;
}

void Utterance::set_options(const Value* options) {
  options_.reset(options->DeepCopy());
}

//
// ExtensionTtsController
//

// static
ExtensionTtsController* ExtensionTtsController::GetInstance() {
  return Singleton<ExtensionTtsController>::get();
}

ExtensionTtsController::ExtensionTtsController()
    : current_utterance_(NULL),
      platform_impl_(NULL) {
}

ExtensionTtsController::~ExtensionTtsController() {
  if (current_utterance_) {
    current_utterance_->Finish();
    delete current_utterance_;
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.
}

void ExtensionTtsController::SpeakOrEnqueue(Utterance* utterance) {
  if (IsSpeaking() && utterance->can_enqueue()) {
    utterance_queue_.push(utterance);
  } else {
    Stop();
    SpeakNow(utterance);
  }
}

void ExtensionTtsController::SpeakNow(Utterance* utterance) {
  const Extension* extension;
  size_t voice_index;
  if (GetMatchingExtensionVoice(utterance, &extension, &voice_index)) {
    current_utterance_ = utterance;
    utterance->set_extension_id(extension->id());

    ExtensionTtsEngineSpeak(utterance, extension, voice_index);

    const std::set<std::string> event_types =
        extension->tts_voices()[voice_index].event_types;
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
  if (!success) {
    utterance->OnTtsEvent(TTS_EVENT_ERROR, -1, GetPlatformImpl()->error());
    delete utterance;
    return;
  }
  current_utterance_ = utterance;
}

void ExtensionTtsController::Stop() {
  if (current_utterance_ && !current_utterance_->extension_id().empty()) {
    ExtensionTtsEngineStop(current_utterance_);
  } else {
    GetPlatformImpl()->clear_error();
    GetPlatformImpl()->StopSpeaking();
  }

  if (current_utterance_)
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, -1, std::string());
  FinishCurrentUtterance();
  ClearUtteranceQueue(true);  // Send events.
}

void ExtensionTtsController::OnTtsEvent(int utterance_id,
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

ListValue* ExtensionTtsController::GetVoices(Profile* profile) {
  ListValue* result_voices = new ListValue();
  ExtensionTtsPlatformImpl* platform_impl = GetPlatformImpl();
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

bool ExtensionTtsController::IsSpeaking() const {
  return current_utterance_ != NULL;
}

void ExtensionTtsController::FinishCurrentUtterance() {
  if (current_utterance_) {
    if (!current_utterance_->finished())
      current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, -1, std::string());
    delete current_utterance_;
    current_utterance_ = NULL;
  }
}

void ExtensionTtsController::SpeakNextUtterance() {
  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  while (!utterance_queue_.empty() && !current_utterance_) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    SpeakNow(utterance);
  }
}

void ExtensionTtsController::ClearUtteranceQueue(bool send_events) {
  while (!utterance_queue_.empty()) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    if (send_events)
      utterance->OnTtsEvent(TTS_EVENT_CANCELLED, -1, std::string());
    else
      utterance->Finish();
    delete utterance;
  }
}

void ExtensionTtsController::SetPlatformImpl(
    ExtensionTtsPlatformImpl* platform_impl) {
  platform_impl_ = platform_impl;
}

int ExtensionTtsController::QueueSize() {
  return static_cast<int>(utterance_queue_.size());
}

ExtensionTtsPlatformImpl* ExtensionTtsController::GetPlatformImpl() {
  if (!platform_impl_)
    platform_impl_ = ExtensionTtsPlatformImpl::GetInstance();
  return platform_impl_;
}
