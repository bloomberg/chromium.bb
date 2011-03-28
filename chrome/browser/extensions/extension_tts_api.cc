// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/float_util.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tts_api.h"
#include "chrome/browser/profiles/profile.h"

namespace util = extension_tts_api_util;

namespace {
const char kSpeechInterruptedError[] = "Utterance interrupted.";
const char kSpeechRemovedFromQueueError[] = "Utterance removed from queue.";
const int kSpeechCheckDelayIntervalMs = 100;
};

namespace events {
const char kOnSpeak[] = "experimental.tts.onSpeak";
const char kOnStop[] = "experimental.tts.onStop";
};  // namespace events

//
// ExtensionTtsPlatformImpl
//

std::string ExtensionTtsPlatformImpl::error() {
  return error_;
}

void ExtensionTtsPlatformImpl::clear_error() {
  error_ = std::string();
}

void ExtensionTtsPlatformImpl::set_error(const std::string& error) {
  error_ = error;
}

//
// Utterance
//

// static
int Utterance::next_utterance_id_ = 0;

Utterance::Utterance(Profile* profile,
                     const std::string& text,
                     DictionaryValue* options,
                     Task* completion_task)
    : profile_(profile),
      id_(next_utterance_id_++),
      text_(text),
      rate_(-1.0),
      pitch_(-1.0),
      volume_(-1.0),
      can_enqueue_(false),
      completion_task_(completion_task) {
  if (!options) {
    // Use all default options.
    options_.reset(new DictionaryValue());
    return;
  }

  options_.reset(options->DeepCopy());

  if (options->HasKey(util::kVoiceNameKey))
    options->GetString(util::kVoiceNameKey, &voice_name_);

  if (options->HasKey(util::kLocaleKey))
    options->GetString(util::kLocaleKey, &locale_);

  if (options->HasKey(util::kGenderKey))
    options->GetString(util::kGenderKey, &gender_);

  if (util::ReadNumberByKey(options, util::kRateKey, &rate_)) {
    if (!base::IsFinite(rate_) || rate_ < 0.0 || rate_ > 1.0)
      rate_ = -1.0;
  }

  if (util::ReadNumberByKey(options, util::kPitchKey, &pitch_)) {
    if (!base::IsFinite(pitch_) || pitch_ < 0.0 || pitch_ > 1.0)
      pitch_ = -1.0;
  }

  if (util::ReadNumberByKey(options, util::kVolumeKey, &volume_)) {
    if (!base::IsFinite(volume_) || volume_ < 0.0 || volume_ > 1.0)
      volume_ = -1.0;
  }

  if (options->HasKey(util::kEnqueueKey))
    options->GetBoolean(util::kEnqueueKey, &can_enqueue_);
}

Utterance::~Utterance() {
  DCHECK_EQ(completion_task_, static_cast<Task *>(NULL));
}

void Utterance::FinishAndDestroy() {
  completion_task_->Run();
  completion_task_ = NULL;
  delete this;
}

//
// ExtensionTtsController
//

// static
ExtensionTtsController* ExtensionTtsController::GetInstance() {
  return Singleton<ExtensionTtsController>::get();
}

ExtensionTtsController::ExtensionTtsController()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      current_utterance_(NULL),
      platform_impl_(NULL) {
}

ExtensionTtsController::~ExtensionTtsController() {
  FinishCurrentUtterance();
  ClearUtteranceQueue();
}

void ExtensionTtsController::SpeakOrEnqueue(Utterance* utterance) {
  if (IsSpeaking() && utterance->can_enqueue()) {
    utterance_queue_.push(utterance);
  } else {
    Stop();
    SpeakNow(utterance);
  }
}

std::string ExtensionTtsController::GetMatchingExtensionId(
    Utterance* utterance) {
  ExtensionService* service = utterance->profile()->GetExtensionService();
  DCHECK(service);
  ExtensionEventRouter* event_router =
      utterance->profile()->GetExtensionEventRouter();
  DCHECK(event_router);

  const ExtensionList* extensions = service->extensions();
  ExtensionList::const_iterator iter;
  for (iter = extensions->begin(); iter != extensions->end(); ++iter) {
    const Extension* extension = *iter;

    if (!event_router->ExtensionHasEventListener(
            extension->id(), events::kOnSpeak) ||
        !event_router->ExtensionHasEventListener(
            extension->id(), events::kOnStop)) {
      continue;
    }

    const std::vector<Extension::TtsVoice>& tts_voices =
        extension->tts_voices();
    for (size_t i = 0; i < tts_voices.size(); ++i) {
      const Extension::TtsVoice& voice = tts_voices[i];
      if (!voice.voice_name.empty() &&
          !utterance->voice_name().empty() &&
          voice.voice_name != utterance->voice_name()) {
        continue;
      }
      if (!voice.locale.empty() &&
          !utterance->locale().empty() &&
          voice.locale != utterance->locale()) {
        continue;
      }
      if (!voice.gender.empty() &&
          !utterance->gender().empty() &&
          voice.gender != utterance->gender()) {
        continue;
      }

      return extension->id();
    }
  }

  return std::string();
}

void ExtensionTtsController::SpeakNow(Utterance* utterance) {
  std::string extension_id = GetMatchingExtensionId(utterance);
  if (!extension_id.empty()) {
    current_utterance_ = utterance;
    utterance->set_extension_id(extension_id);

    ListValue args;
    args.Set(0, Value::CreateStringValue(utterance->text()));

    // Pass through all options to the speech engine, except for
    // "enqueue", which the speech engine doesn't need to handle.
    DictionaryValue* options = static_cast<DictionaryValue*>(
        utterance->options()->DeepCopy());
    if (options->HasKey(util::kEnqueueKey))
      options->Remove(util::kEnqueueKey, NULL);

    args.Set(1, options);
    args.Set(2, Value::CreateIntegerValue(utterance->id()));
    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);

    utterance->profile()->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id,
        events::kOnSpeak,
        json_args,
        utterance->profile(),
        GURL());

    return;
  }

  GetPlatformImpl()->clear_error();
  bool success = GetPlatformImpl()->Speak(
      utterance->text(),
      utterance->locale(),
      utterance->gender(),
      utterance->rate(),
      utterance->pitch(),
      utterance->volume());
  if (!success) {
    utterance->set_error(GetPlatformImpl()->error());
    utterance->FinishAndDestroy();
    return;
  }
  current_utterance_ = utterance;

  // Check to see if it's still speaking; finish the utterance if not and
  // start polling if so. Checking immediately helps to avoid flaky unit
  // tests by forcing them to set expectations for IsSpeaking.
  CheckSpeechStatus();
}

void ExtensionTtsController::Stop() {
  if (current_utterance_ && !current_utterance_->extension_id().empty()) {
    current_utterance_->profile()->GetExtensionEventRouter()->
        DispatchEventToExtension(
            current_utterance_->extension_id(),
            events::kOnStop,
            "[]",
            current_utterance_->profile(),
            GURL());
  } else {
    GetPlatformImpl()->clear_error();
    GetPlatformImpl()->StopSpeaking();
  }

  if (current_utterance_)
    current_utterance_->set_error(kSpeechInterruptedError);
  FinishCurrentUtterance();
  ClearUtteranceQueue();
}

void ExtensionTtsController::OnSpeechFinished(
    int request_id, const std::string& error_message) {
  // We may sometimes receive completion callbacks "late", after we've
  // already finished the utterance (for example because another utterance
  // interrupted or we got a call to Stop). It's also possible that a buggy
  // extension has called this more than once. In either case it's safe to
  // just ignore this call.
  if (!current_utterance_ || request_id != current_utterance_->id())
    return;

  current_utterance_->set_error(error_message);
  FinishCurrentUtterance();
  SpeakNextUtterance();
}

bool ExtensionTtsController::IsSpeaking() const {
  return current_utterance_ != NULL;
}

void ExtensionTtsController::FinishCurrentUtterance() {
  if (current_utterance_) {
    current_utterance_->FinishAndDestroy();
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

void ExtensionTtsController::ClearUtteranceQueue() {
  while (!utterance_queue_.empty()) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    utterance->set_error(kSpeechRemovedFromQueueError);
    utterance->FinishAndDestroy();
  }
}

void ExtensionTtsController::CheckSpeechStatus() {
  if (!current_utterance_)
    return;

  if (!current_utterance_->extension_id().empty())
    return;

  if (GetPlatformImpl()->IsSpeaking() == false) {
    FinishCurrentUtterance();
    SpeakNextUtterance();
  }

  // If we're still speaking something (either the prevoius utterance or
  // a new utterance), keep calling this method after another delay.
  // TODO(dmazzoni): get rid of this as soon as all platform implementations
  // provide completion callbacks rather than only supporting polling.
  if (current_utterance_ && current_utterance_->extension_id().empty()) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, method_factory_.NewRunnableMethod(
            &ExtensionTtsController::CheckSpeechStatus),
        kSpeechCheckDelayIntervalMs);
  }
}

void ExtensionTtsController::SetPlatformImpl(
    ExtensionTtsPlatformImpl* platform_impl) {
  platform_impl_ = platform_impl;
}

ExtensionTtsPlatformImpl* ExtensionTtsController::GetPlatformImpl() {
  if (!platform_impl_)
    platform_impl_ = ExtensionTtsPlatformImpl::GetInstance();
  return platform_impl_;
}

//
// Extension API functions
//

bool ExtensionTtsSpeakFunction::RunImpl() {
  std::string text;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &text));
  DictionaryValue* options = NULL;
  if (args_->GetSize() >= 2)
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));
  Task* completion_task = NewRunnableMethod(
      this, &ExtensionTtsSpeakFunction::SpeechFinished);
  utterance_ = new Utterance(profile(), text, options, completion_task);

  AddRef();  // Balanced in SpeechFinished().
  ExtensionTtsController::GetInstance()->SpeakOrEnqueue(utterance_);
  return true;
}

void ExtensionTtsSpeakFunction::SpeechFinished() {
  error_ = utterance_->error();
  bool success = error_.empty();
  SendResponse(success);
  Release();  // Balanced in RunImpl().
}

bool ExtensionTtsStopSpeakingFunction::RunImpl() {
  ExtensionTtsController::GetInstance()->Stop();
  return true;
}

bool ExtensionTtsIsSpeakingFunction::RunImpl() {
  result_.reset(Value::CreateBooleanValue(
      ExtensionTtsController::GetInstance()->IsSpeaking()));
  return true;
}

bool ExtensionTtsSpeakCompletedFunction::RunImpl() {
  int request_id;
  std::string error_message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id));
  if (args_->GetSize() >= 2)
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &error_message));
  ExtensionTtsController::GetInstance()->OnSpeechFinished(
      request_id, error_message);

  return true;
}
