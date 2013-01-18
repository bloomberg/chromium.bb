// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"

// Handles asynchronous operations such as starting or stopping speech
// recognition in the framework of the extension API state machine.
class SpeechInputAsyncFunction : public AsyncExtensionFunction,
                                 public content::NotificationObserver {
 public:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  SpeechInputAsyncFunction(int start_state, int transition_state,
                           int end_state, int transition_notification);
  virtual ~SpeechInputAsyncFunction();

  // ExtensionFunction:
  virtual void Run() OVERRIDE;
  virtual bool RunImpl() = 0;

 private:
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
  DECLARE_EXTENSION_FUNCTION("experimental.speechInput.start",
                             EXPERIMENTAL_SPEECHINPUT_START)

  StartSpeechInputFunction();

 protected:
  // SpeechInputAsyncFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual ~StartSpeechInputFunction() {}
};

// Implements experimental.speechInput.stop.
class StopSpeechInputFunction : public SpeechInputAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.speechInput.stop",
                             EXPERIMENTAL_SPEECHINPUT_STOP)

  StopSpeechInputFunction();

 protected:
  // SpeechInputAsyncFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual ~StopSpeechInputFunction() {}
};

// Implements experimental.speechInput.isRecording.
class IsRecordingSpeechInputFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.speechInput.isRecording",
                             EXPERIMENTAL_SPEECHINPUT_ISRECORDING)

  // Called back from SpeechInputExtensionManager in the UI thread.
  void SetIsRecordingResult(bool result);

 protected:
  // ExtensionFunction:
  virtual void Run() OVERRIDE;
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual ~IsRecordingSpeechInputFunction() {}
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_API_H_
