// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_

#include "base/basictypes.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "ipc/ipc_message.h"

namespace speech_input {

// This is the gatekeeper for speech recognition in the browser process. It
// handles requests received from various render views and makes sure only one
// of them can use speech recognition at a time. It also sends recognition
// results and status events to the render views when required.
class SpeechInputManager {
 public:
  // Implemented by the dispatcher host to relay events to the render views.
  class Delegate {
  public:
    virtual void SetRecognitionResult(const SpeechInputCallerId& caller_id,
                                      const string16& value) = 0;
    virtual void DidCompleteRecording(const SpeechInputCallerId& caller_id) = 0;
    virtual void DidCompleteRecognition(
        const SpeechInputCallerId& caller_id) = 0;
  };

  // Factory method to create new instances.
  static SpeechInputManager* Create(Delegate* delegate);
  // Factory method definition useful for tests.
  typedef SpeechInputManager* (FactoryMethod)(Delegate*);

  virtual ~SpeechInputManager() {}

  // Handlers for requests from render views.
  virtual void StartRecognition(const SpeechInputCallerId& caller_id)  = 0;
  virtual void CancelRecognition(const SpeechInputCallerId& caller_id) = 0;
  virtual void StopRecording(const SpeechInputCallerId& caller_id) = 0;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechInputManager::Delegate SpeechInputManagerDelegate;

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
