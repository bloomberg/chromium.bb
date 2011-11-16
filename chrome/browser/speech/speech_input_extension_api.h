// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"

// Handles asynchronous operations such as starting or stopping speech
// recognition in the framework of the extension API state machine.
class SpeechInputAsyncFunction : public AsyncExtensionFunction,
                                 public content::NotificationObserver {
 protected:
  SpeechInputAsyncFunction(int start_state, int transition_state,
                           int end_state, int transition_notification);
  virtual ~SpeechInputAsyncFunction();

  virtual void Run() OVERRIDE;
  virtual bool RunImpl() = 0;

 private:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // To be defined on construction by derived classes.
  int start_state_;
  int transition_state_;
  int end_state_;
  int transition_notification_;

  content::NotificationRegistrar registrar_;
  bool expecting_transition_;
  bool failed_;
};

// Implements experimental.speechInput.start.
class StartSpeechInputFunction : public SpeechInputAsyncFunction {
 public:
  StartSpeechInputFunction();
  virtual ~StartSpeechInputFunction() {}

 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.speechInput.start");
};

// Implements experimental.speechInput.stop.
class StopSpeechInputFunction : public SpeechInputAsyncFunction {
 public:
  StopSpeechInputFunction();
  virtual ~StopSpeechInputFunction() {}

 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.speechInput.stop");
};

// Implements experimental.speechInput.isRecording.
class IsRecordingSpeechInputFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.speechInput.isRecording");
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_API_H_
