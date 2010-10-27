// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tts_api.h"

#include <string>

#include "base/float_util.h"
#include "base/message_loop.h"
#include "base/values.h"

namespace util = extension_tts_api_util;

namespace {
const char kCrosLibraryNotLoadedError[] =
    "Cros shared library not loaded.";
const int kSpeechCheckDelayIntervalMs = 100;
};

// static
ExtensionTtsController* ExtensionTtsController::GetInstance() {
  return Singleton<ExtensionTtsController>::get();
}

ExtensionTtsController::ExtensionTtsController()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      current_utterance_(NULL),
      platform_impl_(NULL) {
}

void ExtensionTtsController::SpeakOrEnqueue(
    Utterance* utterance, bool can_enqueue) {
  if (IsSpeaking() && can_enqueue) {
    utterance_queue_.push(utterance);
  } else {
    Stop();
    SpeakNow(utterance);
  }
}

void ExtensionTtsController::SpeakNow(Utterance* utterance) {
  GetPlatformImpl()->clear_error();
  bool success = GetPlatformImpl()->Speak(
      utterance->text,
      utterance->language,
      utterance->gender,
      utterance->rate,
      utterance->pitch,
      utterance->volume);
  if (!success) {
    utterance->error = GetPlatformImpl()->error();
    utterance->failure_task->Run();
    delete utterance->success_task;
    delete utterance;
    return;
  }
  current_utterance_ = utterance;

  // Post a task to check if this utterance has completed after a delay.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, method_factory_.NewRunnableMethod(
          &ExtensionTtsController::CheckSpeechStatus),
      kSpeechCheckDelayIntervalMs);
}

void ExtensionTtsController::Stop() {
  GetPlatformImpl()->clear_error();
  GetPlatformImpl()->StopSpeaking();

  FinishCurrentUtterance();
  ClearUtteranceQueue();
}

bool ExtensionTtsController::IsSpeaking() const {
  return current_utterance_ != NULL;
}

void ExtensionTtsController::FinishCurrentUtterance() {
  if (current_utterance_) {
    current_utterance_->success_task->Run();
    delete current_utterance_->failure_task;
    delete current_utterance_;
    current_utterance_ = NULL;
  }
}

void ExtensionTtsController::ClearUtteranceQueue() {
  while (!utterance_queue_.empty()) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    utterance->success_task->Run();
    delete utterance->failure_task;
    delete utterance;
  }
}

void ExtensionTtsController::CheckSpeechStatus() {
  if (!current_utterance_)
    return;

  if (GetPlatformImpl()->IsSpeaking() == false) {
    FinishCurrentUtterance();

    // Start speaking the next utterance in the queue.  Keep trying in case
    // one fails but there are still more in the queue to try.
    while (!utterance_queue_.empty() && !current_utterance_) {
      Utterance* utterance = utterance_queue_.front();
      utterance_queue_.pop();
      SpeakNow(utterance);
    }
  }

  // If we're still speaking something (either the prevoius utterance or
  // a new utterance), keep calling this method after another delay.
  if (current_utterance_) {
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
  utterance_ = new ExtensionTtsController::Utterance();
  bool can_enqueue = false;

  DictionaryValue* speak_options = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &utterance_->text));

  if (args_->GetDictionary(1, &speak_options)) {
    if (speak_options->HasKey(util::kLanguageNameKey)) {
      speak_options->GetString(util::kLanguageNameKey, &utterance_->language);
    }

    if (speak_options->HasKey(util::kGenderKey)) {
      speak_options->GetString(util::kGenderKey, &utterance_->gender);
    }

    if (speak_options->HasKey(util::kEnqueueKey)) {
      speak_options->GetBoolean(util::kEnqueueKey, &can_enqueue);
    }

    if (util::ReadNumberByKey(
            speak_options, util::kRateKey, &utterance_->rate)) {
      if (!base::IsFinite(utterance_->rate) ||
          utterance_->rate < 0.0 ||
          utterance_->rate > 1.0) {
        utterance_->rate = -1.0;
      }
    }

    if (util::ReadNumberByKey(
            speak_options, util::kPitchKey, &utterance_->pitch)) {
      if (!base::IsFinite(utterance_->pitch) ||
          utterance_->pitch < 0.0 ||
          utterance_->pitch > 1.0) {
        utterance_->pitch = -1.0;
      }
    }

    if (util::ReadNumberByKey(
            speak_options, util::kVolumeKey, &utterance_->volume)) {
      if (!base::IsFinite(utterance_->volume) ||
          utterance_->volume < 0.0 ||
          utterance_->volume > 1.0) {
        utterance_->volume = -1.0;
      }
    }
  }

  AddRef();  // Balanced in SpeechFinished().
  utterance_->success_task = NewRunnableMethod(
      this, &ExtensionTtsSpeakFunction::SpeechFinished, true);
  utterance_->failure_task = NewRunnableMethod(
      this, &ExtensionTtsSpeakFunction::SpeechFinished, false);
  ExtensionTtsController::GetInstance()->SpeakOrEnqueue(
      utterance_, can_enqueue);
  return true;
}

void ExtensionTtsSpeakFunction::SpeechFinished(bool success) {
  error_ = utterance_->error;
  SendResponse(success);
  Release();  // Balanced in Speak().
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
