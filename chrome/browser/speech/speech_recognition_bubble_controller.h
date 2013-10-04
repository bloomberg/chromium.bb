// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/speech/speech_recognition_bubble.h"
#include "ui/gfx/rect.h"

namespace speech {

// This class handles the speech recognition popup UI on behalf of
// SpeechRecognitionManager, which invokes methods on the IO thread, processing
// those requests on the UI thread. At most one bubble can be active.
class SpeechRecognitionBubbleController
    : public base::RefCountedThreadSafe<SpeechRecognitionBubbleController>,
      public SpeechRecognitionBubbleDelegate {
 public:
  // All methods of this delegate are called on the IO thread.
  class Delegate {
   public:
    // Invoked when the user clicks on a button in the speech recognition UI.
    virtual void InfoBubbleButtonClicked(
        int session_id, SpeechRecognitionBubble::Button button) = 0;

    // Invoked when the user clicks outside the speech recognition info bubble
    // causing it to close and input focus to change.
    virtual void InfoBubbleFocusChanged(int session_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit SpeechRecognitionBubbleController(Delegate* delegate);

  // Creates and shows a new speech recognition UI bubble in warmup mode.
  void CreateBubble(int session_id,
                    int render_process_id,
                    int render_view_id,
                    const gfx::Rect& element_rect);

  // Indicates to the user that audio recording is in progress.
  void SetBubbleRecordingMode();

  // Indicates to the user that recognition is in progress.
  void SetBubbleRecognizingMode();

  // Displays the given string with the 'Try again' and 'Cancel' buttons.
  void SetBubbleMessage(const string16& text);

  // Checks whether the bubble is active and is showing a message.
  bool IsShowingMessage() const;

  // Updates the current captured audio volume displayed on screen.
  void SetBubbleInputVolume(float volume, float noise_volume);

  // Closes the bubble.
  void CloseBubble();

  // This is the only method that can be called from the UI thread.
  void CloseBubbleForRenderViewOnUIThread(int render_process_id,
                                          int render_view_id);

  // Retrieves the session ID associated to the active bubble (if any).
  // Returns 0 if no bubble is currently shown.
  int GetActiveSessionID();

  // SpeechRecognitionBubble::Delegate methods.
  virtual void InfoBubbleButtonClicked(
      SpeechRecognitionBubble::Button button) OVERRIDE;
  virtual void InfoBubbleFocusChanged() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<SpeechRecognitionBubbleController>;

  // The various calls received by this object and handled on the UI thread.
  enum RequestType {
    REQUEST_CREATE,
    REQUEST_SET_RECORDING_MODE,
    REQUEST_SET_RECOGNIZING_MODE,
    REQUEST_SET_MESSAGE,
    REQUEST_SET_INPUT_VOLUME,
    REQUEST_CLOSE,
  };

  struct UIRequest {
    RequestType type;
    string16 message;
    gfx::Rect element_rect;
    float volume;
    float noise_volume;
    int render_process_id;
    int render_view_id;

    explicit UIRequest(RequestType type_value);
    ~UIRequest();
  };

  virtual ~SpeechRecognitionBubbleController();

  void InvokeDelegateButtonClicked(SpeechRecognitionBubble::Button button);
  void InvokeDelegateFocusChanged();
  void ProcessRequestInUiThread(const UIRequest& request);

  // The following are accessed only on the IO thread.
  Delegate* delegate_;
  RequestType last_request_issued_;

  // The following are accessed only on the UI thread.
  scoped_ptr<SpeechRecognitionBubble> bubble_;

  // The following are accessed on both IO and  UI threads.
  base::Lock lock_;

  // The session id for currently visible bubble.
  int current_bubble_session_id_;

  // The render process and view ids for the currently visible bubble.
  int current_bubble_render_process_id_;
  int current_bubble_render_view_id_;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognitionBubbleController::Delegate
    SpeechRecognitionBubbleControllerDelegate;

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_CONTROLLER_H_
