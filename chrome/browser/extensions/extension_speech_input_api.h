// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SPEECH_INPUT_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SPEECH_INPUT_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "content/common/notification_service.h"

class ExtensionSpeechInputManager;

// Handles asynchronous operations such as starting or stopping speech
// recognition in the framework of the extension API state machine.
class SpeechInputAsyncFunction : public AsyncExtensionFunction,
                                 public NotificationObserver {
 protected:
  SpeechInputAsyncFunction();

  virtual void Run() OVERRIDE;
  virtual bool RunImpl() = 0;

 private:
  // NotificationObserver.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;
};

// Implements experimental.speechInput.start.
class StartSpeechInputFunction : public SpeechInputAsyncFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.speechInput.start");
};

// Implements experimental.speechInput.stop.
class StopSpeechInputFunction : public SpeechInputAsyncFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.speechInput.stop");
};

// Implements experimental.speechInput.isRecording.
class IsRecordingSpeechInputFunction : public SyncIOThreadExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.speechInput.isRecording");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SPEECH_INPUT_API_H_
