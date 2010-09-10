// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_CONTROLLER_H_

#include <map>

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
// those requests in the UI thread. There could be multiple bubble objects alive
// at the same time but only one of them is visible to the user. User actions on
// that bubble are reported to the delegate.
class SpeechInputBubbleController
    : public base::RefCountedThreadSafe<SpeechInputBubbleController>,
      public SpeechInputBubbleDelegate {
 public:
  // All methods of this delegate are called in the IO thread.
  class Delegate {
   public:
    // Invoked when the user clicks on a button in the speech input UI.
    virtual void InfoBubbleButtonClicked(int caller_id,
                                         SpeechInputBubble::Button button) = 0;

    // Invoked when the user clicks outside the speech input info bubble causing
    // it to close and input focus to change.
    virtual void InfoBubbleFocusChanged(int caller_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit SpeechInputBubbleController(Delegate* delegate);
  virtual ~SpeechInputBubbleController();

  // Creates a new speech input UI bubble. One of the SetXxxx methods below need
  // to be called to specify what to display.
  void CreateBubble(int caller_id,
                    int render_process_id,
                    int render_view_id,
                    const gfx::Rect& element_rect);

  // Indicates to the user that audio recording is in progress. This also makes
  // the bubble visible if not already visible.
  void SetBubbleRecordingMode(int caller_id);

  // Indicates to the user that recognition is in progress. If the bubble is
  // hidden, |Show| must be called to make it appear on screen.
  void SetBubbleRecognizingMode(int caller_id);

  // Displays the given string with the 'Try again' and 'Cancel' buttons. If the
  // bubble is hidden, |Show| must be called to make it appear on screen.
  void SetBubbleMessage(int caller_id, const string16& text);

  void CloseBubble(int caller_id);

  // SpeechInputBubble::Delegate methods.
  virtual void InfoBubbleButtonClicked(SpeechInputBubble::Button button);
  virtual void InfoBubbleFocusChanged();

 private:
  void InvokeDelegateButtonClicked(int caller_id,
                                   SpeechInputBubble::Button button);
  void InvokeDelegateFocusChanged(int caller_id);
  void SetBubbleRecordingModeOrMessage(int caller_id, const string16& text);

  // Only accessed in the IO thread.
  Delegate* delegate_;

  //*** The following are accessed only in the UI thread.

  // The caller id for currently visible bubble (since only one bubble is
  // visible at any time).
  int current_bubble_caller_id_;

  // Map of caller-ids to bubble objects. The bubbles are weak pointers owned by
  // this object and get destroyed by |CloseBubble|.
  std::map<int, SpeechInputBubble*> bubbles_;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechInputBubbleController::Delegate
    SpeechInputBubbleControllerDelegate;

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_CONTROLLER_H_
