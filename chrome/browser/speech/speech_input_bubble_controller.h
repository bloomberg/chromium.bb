// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/speech/speech_input_bubble.h"

namespace gfx {
class Rect;
}

namespace speech_input {

// This class handles the speech input popup UI on behalf of SpeechInputManager.
// SpeechInputManager invokes methods in the IO thread and this class processes
// those requests in the UI thread. There is only 1 speech input bubble shown to
// the user at a time. User actions on that bubble are reported to the delegate.
class SpeechInputBubbleController
    : public base::RefCountedThreadSafe<SpeechInputBubbleController>,
      public SpeechInputBubbleDelegate {
 public:
  // All methods of this delegate are called in the IO thread.
  class Delegate {
   public:
    // Invoked when the user cancels speech recognition by clicking on the
    // cancel button or related action in the speech input UI.
    virtual void RecognitionCancelled(int caller_id) = 0;

    // Invoked when the user clicks outside the speech input info bubble causing
    // it to close and input focus to change.
    virtual void SpeechInputFocusChanged(int caller_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit SpeechInputBubbleController(Delegate* delegate);

  // Creates a new speech input bubble and displays it in the UI.
  void CreateBubble(int caller_id,
                    int render_process_id,
                    int render_view_id,
                    const gfx::Rect& element_rect);

  // Sets the bubble to show that recording completed and recognition is in
  // progress.
  void SetBubbleToRecognizingMode(int caller_id);

  void CloseBubble(int caller_id);

  // SpeechInputBubble::Delegate methods.
  virtual void RecognitionCancelled();
  virtual void InfoBubbleClosed();

 private:
  void InvokeDelegateRecognitionCancelled(int caller_id);
  void InvokeDelegateFocusChanged(int caller_id);

  // Only accessed in the IO thread.
  Delegate* delegate_;

  // Only accessed in the UI thread.
  int current_bubble_caller_id_;
  scoped_ptr<SpeechInputBubble> bubble_;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechInputBubbleController::Delegate
    SpeechInputBubbleControllerDelegate;

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_CONTROLLER_H_
