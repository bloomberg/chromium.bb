// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_speech_input_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_speech_input_manager.h"
#include "chrome/browser/profiles/profile.h"

SpeechInputAsyncFunction::SpeechInputAsyncFunction() {
}

void SpeechInputAsyncFunction::Run() {

  // TODO(leandrogracia): implement the management of the asynchronous
  // operations involved and the corresponding state transitions.
  RunImpl();

  SendResponse(true);
}

void SpeechInputAsyncFunction::Observe(int type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  // TODO(leandrogracia): to be implemented.
}

bool StartSpeechInputFunction::RunImpl() {
  ExtensionSpeechInputManager::GetForProfile(profile())->Start(GetExtension());
  return true;
}

bool StopSpeechInputFunction::RunImpl() {
  ExtensionSpeechInputManager::GetForProfile(profile())->Stop(GetExtension());
  return true;
}

bool IsRecordingSpeechInputFunction::RunImpl() {
  // TODO(leandrogracia): to be implemented.
  result_.reset(Value::CreateBooleanValue(false));
  return true;
}
