// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/speech_input/extension_speech_input_api.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/speech_input/extension_speech_input_api_constants.h"
#include "chrome/browser/extensions/speech_input/extension_speech_input_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace constants = extension_speech_input_api_constants;

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

SpeechInputAsyncFunction::~SpeechInputAsyncFunction() {
}

void SpeechInputAsyncFunction::Run() {
  if (failed_) {
    registrar_.RemoveAll();
    SendResponse(false);
    return;
  }

  ExtensionSpeechInputManager::State state =
      ExtensionSpeechInputManager::GetForProfile(profile())->state();

  // RunImpl should be always called once independently of the state we're in,
  // otherwise we might miss requestDenied error situations.
  if (!expecting_transition_) {
    ExtensionSpeechInputManager::State state_before_call = state;

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

    state = ExtensionSpeechInputManager::GetForProfile(profile())->state();
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
    ExtensionSpeechInputManager::ExtensionError* error_details =
        content::Details<ExtensionSpeechInputManager::ExtensionError>(
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
    : SpeechInputAsyncFunction(ExtensionSpeechInputManager::kIdle,
          ExtensionSpeechInputManager::kStarting,
          ExtensionSpeechInputManager::kRecording,
          chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STARTED) {
}

bool StartSpeechInputFunction::RunImpl() {
  std::string language;
  std::string grammar = constants::kDefaultGrammar;
  bool filter_profanities = constants::kDefaultFilterProfanities;

  if (!args_->empty()) {
    DictionaryValue *options;
    if (!args_->GetDictionary(0, &options))
      return false;
    DCHECK(options);

    if (options->HasKey(constants::kLanguageKey))
      options->GetString(constants::kLanguageKey, &language);
    if (options->HasKey(constants::kGrammarKey))
      options->GetString(constants::kGrammarKey, &grammar);

    if (options->HasKey(constants::kFilterProfanitiesKey)) {
      options->GetBoolean(constants::kFilterProfanitiesKey,
          &filter_profanities);
    }
  }

  // Use the application locale if the language is empty or not provided.
  if (language.empty()) {
    language = g_browser_process->GetApplicationLocale();
    VLOG(1) << "Language not specified. Using application locale " << language;
  }

  return ExtensionSpeechInputManager::GetForProfile(profile())->Start(
      extension_id(), language, grammar, filter_profanities, &error_);
}

StopSpeechInputFunction::StopSpeechInputFunction()
    : SpeechInputAsyncFunction(ExtensionSpeechInputManager::kRecording,
          ExtensionSpeechInputManager::kStopping,
          ExtensionSpeechInputManager::kIdle,
          chrome::NOTIFICATION_EXTENSION_SPEECH_INPUT_RECORDING_STOPPED) {
}

bool StopSpeechInputFunction::RunImpl() {
  return ExtensionSpeechInputManager::GetForProfile(
      profile())->Stop(extension_id(), &error_);
}

bool IsRecordingSpeechInputFunction::RunImpl() {
  // Do not access the AudioManager directly here to ensure the proper
  // IsRecording behaviour in the API tests.
  result_.reset(Value::CreateBooleanValue(
      ExtensionSpeechInputManager::GetForProfile(profile())->IsRecording()));
  return true;
}
