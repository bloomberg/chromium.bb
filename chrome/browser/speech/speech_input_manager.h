// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "ipc/ipc_message.h"

namespace speech_input {

// This is the gatekeeper for speech recognition in the browser process. It
// handles requests received from various render views and makes sure only one
// of them can use speech recognition at a time. It also sends recognition
// results and status events to the render views when required.
class SpeechInputManager {
 public:
  // Implemented by the dispatcher host to relay events to the render views.
  class Listener {
  public:
    virtual void SetRecognitionResult(int render_view_id,
                                      const string16& value) = 0;
    virtual void DidCompleteRecording(int render_view_id) = 0;
    virtual void DidCompleteRecognition(int render_view_id) = 0;
  };

  // Factory method to create new instances.
  static SpeechInputManager* Create(Listener* listener);
  // Factory method definition useful for tests.
  typedef SpeechInputManager* (FactoryMethod)(Listener*);

  virtual ~SpeechInputManager() {}

  // Handlers for requests from render views.
  virtual void StartRecognition(int render_view_id)  = 0;
  virtual void CancelRecognition(int render_view_id) = 0;
  virtual void StopRecording(int render_view_id) = 0;
};

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
