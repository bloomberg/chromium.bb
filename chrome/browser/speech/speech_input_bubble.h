// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_H_
#pragma once

#include "gfx/rect.h"

class TabContents;

// SpeechInputBubble displays a popup info bubble during speech recognition,
// points to the html element which requested speech input and shows recognition
// progress events. The popup is closed by the user clicking anywhere outside
// the popup window, or by the caller destroying this object.
class SpeechInputBubble {
 public:
  // Informs listeners of user actions in the bubble.
  class Delegate {
   public:
    // Invoked when the user cancels speech recognition by clicking on the
    // cancel button. The InfoBubble is still active and the caller should close
    // it if necessary.
    virtual void RecognitionCancelled() = 0;

    // Invoked when the user clicks outside the InfoBubble causing it to close.
    // The InfoBubble window is no longer visible on screen and the caller can
    // free the InfoBubble instance. This callback is not issued if the bubble
    // got closed because the object was destroyed by the caller.
    virtual void InfoBubbleClosed() = 0;

   protected:
    virtual ~Delegate() {
    }
  };

  // Factory method to create new instances.
  // Creates and displays the bubble.
  // |tab_contents| is the TabContents hosting the page.
  // |element_rect| is the display bounds of the html element requesting speech
  // input (in page coordinates).
  static SpeechInputBubble* Create(TabContents* tab_contents,
                                   Delegate* delegate,
                                   const gfx::Rect& element_rect);
  virtual ~SpeechInputBubble() {}

  // Indicates to the user that recognition is in progress.
  virtual void SetRecognizingMode() = 0;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechInputBubble::Delegate SpeechInputBubbleDelegate;

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_H_
