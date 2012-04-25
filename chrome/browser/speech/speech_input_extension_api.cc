// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_extension_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_input_extension_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace {

const char kLanguageKey[] = "language";
const char kGrammarKey[] = "grammar";
const char kFilterProfanitiesKey[] = "filterProfanities";

const char kDefaultGrammar[] = "builtin:dictation";
const bool kDefaultFilterProfanities = true;

}  // anonymous namespace

SpeechInputAsyncFunction::SpeechInputAsyncFunction(
    int start_state,
    int transition_state,
    int end_state,
    int transition_notification)
    : start_state_(start_state),
      transition_state_(transition_state),
      end_state_(end_state),
      transition_notification_(transition_notification),
      expecting_transition_(false),
      failed_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_FAILED,
      content::Source<Profile>(profile()));
}

SpeechInputAsyncFunction::~SpeechInputAsyncFunction() {}

void SpeechInputAsyncFunction::Run() {
  if (failed_) {
    registrar_.RemoveAll();
    SendResponse(false);
    return;
  }

  SpeechInputExtensionManager::State state =
      SpeechInputExtensionManager::GetForProfile(profile())->state();

  // RunImpl should be always called once independently of the state we're in,
  // otherwise we might miss requestDenied error situations.
  if (!expecting_transition_) {
    SpeechInputExtensionManager::State state_before_call = state;

    // Register before RunImpl to ensure it's received if generated.
    if (state_before_call == start_state_) {
      registrar_.Add(this, transition_notification_,
          content::Source<Profile>(profile()));
      AddRef(); // Balanced in Observe().
    }

    if (!RunImpl()) {
      registrar_.RemoveAll();
      SendResponse(false);
      return;
    }

    // RunImpl should always return false and set the appropriate error code
    // when called in a state different to the start one.
    DCHECK_EQ(state_before_call, start_state_);

    state = SpeechInputExtensionManager::GetForProfile(profile())->state();
    DCHECK_EQ(state, transition_state_);
    expecting_transition_ = true;
  }

  if (state == transition_state_)
    return;

  DCHECK_EQ(state, end_state_);
  registrar_.RemoveAll();
  SendResponse(true);
}

void SpeechInputAsyncFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(profile(), content::Source<Profile>(source).ptr());

  if (type == chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_FAILED) {
    SpeechInputExtensionManager::ExtensionError* error_details =
        content::Details<SpeechInputExtensionManager::ExtensionError>(
            details).ptr();
    if (error_details->extension_id_ != extension_id())
      return;

    error_ = error_details->error_;
    failed_ = true;
  } else {
    DCHECK_EQ(type, transition_notification_);
    if (*content::Details<std::string>(details).ptr() != extension_id())
      return;
    DCHECK(expecting_transition_);
  }

  Run();
  Release(); // Balanced in Run().
}

StartSpeechInputFunction::StartSpeechInputFunction()
    : SpeechInputAsyncFunction(SpeechInputExtensionManager::kIdle,
          SpeechInputExtensionManager::kStarting,
          SpeechInputExtensionManager::kRecording,
          chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STARTED) {
}

bool StartSpeechInputFunction::RunImpl() {
  std::string language;
  std::string grammar = kDefaultGrammar;
  bool filter_profanities = kDefaultFilterProfanities;

  if (!args_->empty()) {
    DictionaryValue *options;
    if (!args_->GetDictionary(0, &options))
      return false;
    DCHECK(options);

    if (options->HasKey(kLanguageKey))
      options->GetString(kLanguageKey, &language);
    if (options->HasKey(kGrammarKey))
      options->GetString(kGrammarKey, &grammar);

    if (options->HasKey(kFilterProfanitiesKey)) {
      options->GetBoolean(kFilterProfanitiesKey,
          &filter_profanities);
    }
  }

  // Use the application locale if the language is empty or not provided.
  if (language.empty()) {
    language = g_browser_process->GetApplicationLocale();
    VLOG(1) << "Language not specified. Using application locale " << language;
  }

  return SpeechInputExtensionManager::GetForProfile(profile())->Start(
      extension_id(), language, grammar, filter_profanities, &error_);
}

StopSpeechInputFunction::StopSpeechInputFunction()
    : SpeechInputAsyncFunction(SpeechInputExtensionManager::kRecording,
          SpeechInputExtensionManager::kStopping,
          SpeechInputExtensionManager::kIdle,
          chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STOPPED) {
}

bool StopSpeechInputFunction::RunImpl() {
  return SpeechInputExtensionManager::GetForProfile(
      profile())->Stop(extension_id(), &error_);
}

void IsRecordingSpeechInputFunction::SetResult(bool result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  result_.reset(Value::CreateBooleanValue(result));
  SendResponse(true);
}

void IsRecordingSpeechInputFunction::Run() {
  SpeechInputExtensionManager::GetForProfile(profile())->IsRecording(
      base::Bind(&IsRecordingSpeechInputFunction::SetResult, this));
}

bool IsRecordingSpeechInputFunction::RunImpl() {
  // The operation needs to be asynchronous because of thread requirements.
  // This method does nothing, but it needs to be implemented anyway.
  return true;
}
