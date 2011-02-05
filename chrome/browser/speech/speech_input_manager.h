// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_

#include "base/basictypes.h"
#include "chrome/common/speech_input_result.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/rect.h"

namespace speech_input {

// This is the gatekeeper for speech recognition in the browser process. It
// handles requests received from various render views and makes sure only one
// of them can use speech recognition at a time. It also sends recognition
// results and status events to the render views when required.
// This class is a singleton and accessed via the Get method.
class SpeechInputManager {
 public:
  // Implemented by the dispatcher host to relay events to the render views.
  class Delegate {
   public:
    virtual void SetRecognitionResult(
        int caller_id,
        const SpeechInputResultArray& result) = 0;
    virtual void DidCompleteRecording(int caller_id) = 0;
    virtual void DidCompleteRecognition(int caller_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Whether the speech input feature is enabled, based on the browser channel
  // information and command line flags.
  static bool IsFeatureEnabled();

  // Factory method to access the singleton. We have this method here instead of
  // using Singleton directly in the calling code to aid tests in injection
  // mocks.
  static SpeechInputManager* Get();
  // Factory method definition useful for tests.
  typedef SpeechInputManager* (AccessorMethod)();

  virtual ~SpeechInputManager() {}

  // Handlers for requests from render views.

  // |delegate| is a weak pointer and should remain valid until
  // its |DidCompleteRecognition| method is called or recognition is cancelled.
  // |render_process_id| is the ID of the renderer process initiating the
  // request.
  // |element_rect| is the display bounds of the html element requesting speech
  // input (in page coordinates).
  virtual void StartRecognition(Delegate* delegate,
                                int caller_id,
                                int render_process_id,
                                int render_view_id,
                                const gfx::Rect& element_rect,
                                const std::string& language,
                                const std::string& grammar,
                                const std::string& origin_url)  = 0;
  virtual void CancelRecognition(int caller_id) = 0;
  virtual void StopRecording(int caller_id) = 0;

  virtual void CancelAllRequestsWithDelegate(Delegate* delegate) = 0;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechInputManager::Delegate SpeechInputManagerDelegate;

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
