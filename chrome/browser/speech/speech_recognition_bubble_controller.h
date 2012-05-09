// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_CONTROLLER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/speech/speech_recognition_bubble.h"
#include "content/public/browser/notification_observer.h"

namespace gfx {
class Rect;
}

namespace content {
class NotificationRegistrar;
}

namespace speech {

// This class handles the speech recognition popup UI on behalf of
// SpeechRecognitionManager, which invokes methods in the IO thread, processing
// those requests in the UI thread. There could be multiple bubble objects alive
// at the same time but only one of them is visible to the user. User actions on
// that bubble are reported to the delegate.
class SpeechRecognitionBubbleController
    : public base::RefCountedThreadSafe<SpeechRecognitionBubbleController>,
      public SpeechRecognitionBubbleDelegate,
      public content::NotificationObserver {
 public:
  // All methods of this delegate are called in the IO thread.
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

  // Creates a new speech recognition UI bubble. One of the SetXxxx methods
  // below need to be called to specify what to display.
  void CreateBubble(int session_id,
                    int render_process_id,
                    int render_view_id,
                    const gfx::Rect& element_rect);

  // Indicates to the user that audio hardware is warming up. This also makes
  // the bubble visible if not already visible.
  void SetBubbleWarmUpMode(int session_id);

  // Indicates to the user that audio recording is in progress. This also makes
  // the bubble visible if not already visible.
  void SetBubbleRecordingMode(int session_id);

  // Indicates to the user that recognition is in progress. If the bubble is
  // hidden, |Show| must be called to make it appear on screen.
  void SetBubbleRecognizingMode(int session_id);

  // Displays the given string with the 'Try again' and 'Cancel' buttons. If the
  // bubble is hidden, |Show| must be called to make it appear on screen.
  void SetBubbleMessage(int session_id, const string16& text);

  // Updates the current captured audio volume displayed on screen.
  void SetBubbleInputVolume(int session_id, float volume, float noise_volume);

  void CloseBubble(int session_id);

  // SpeechRecognitionBubble::Delegate methods.
  virtual void InfoBubbleButtonClicked(
      SpeechRecognitionBubble::Button button) OVERRIDE;
  virtual void InfoBubbleFocusChanged() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<SpeechRecognitionBubbleController>;

  // The various calls received by this object and handled in the UI thread.
  enum RequestType {
    REQUEST_SET_WARM_UP_MODE,
    REQUEST_SET_RECORDING_MODE,
    REQUEST_SET_RECOGNIZING_MODE,
    REQUEST_SET_MESSAGE,
    REQUEST_SET_INPUT_VOLUME,
    REQUEST_CLOSE,
  };

  enum ManageSubscriptionAction {
    BUBBLE_ADDED,
    BUBBLE_REMOVED
  };

  virtual ~SpeechRecognitionBubbleController();

  void InvokeDelegateButtonClicked(int session_id,
                                   SpeechRecognitionBubble::Button button);
  void InvokeDelegateFocusChanged(int session_id);
  void ProcessRequestInUiThread(int session_id,
                                RequestType type,
                                const string16& text,
                                float volume,
                                float noise_volume);

  // Called whenever a bubble was added to or removed from the list. If the
  // bubble was being added, this method registers for close notifications with
  // the WebContents if this was the first bubble for the tab. Similarly if the
  // bubble was being removed, this method unregisters from WebContents if this
  // was the last bubble associated with that tab.
  void UpdateTabContentsSubscription(int session_id,
                                     ManageSubscriptionAction action);

  // Only accessed in the IO thread.
  Delegate* delegate_;

  // *** The following are accessed only in the UI thread.

  // The session id for currently visible bubble (since only one bubble is
  // visible at any time).
  int current_bubble_session_id_;

  // Map of session-ids to bubble objects. The bubbles are weak pointers owned
  // by this object and get destroyed by |CloseBubble|.
  typedef std::map<int, SpeechRecognitionBubble*> BubbleSessionIdMap;
  BubbleSessionIdMap bubbles_;

  scoped_ptr<content::NotificationRegistrar> registrar_;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognitionBubbleController::Delegate
    SpeechRecognitionBubbleControllerDelegate;

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_CONTROLLER_H_
