// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_H_
#pragma once

#include <vector>

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"

namespace gfx {
class Rect;
}
class SkBitmap;
class TabContents;

// SpeechInputBubble displays a popup info bubble during speech recognition,
// points to the html element which requested speech input and shows recognition
// progress events. The popup is closed by the user clicking anywhere outside
// the popup window, or by the caller destroying this object.
class SpeechInputBubble {
 public:
  // The various buttons which may be part of the bubble.
  enum Button {
    BUTTON_TRY_AGAIN,
    BUTTON_CANCEL
  };

  // Informs listeners of user actions in the bubble.
  class Delegate {
   public:
    // Invoked when the user selects a button in the info bubble. The InfoBubble
    // is still active and the caller should close it if necessary.
    virtual void InfoBubbleButtonClicked(Button button) = 0;

    // Invoked when the user clicks outside the InfoBubble causing it to close.
    // The InfoBubble window is no longer visible on screen and the caller can
    // free the InfoBubble instance. This callback is not issued if the bubble
    // got closed because the object was destroyed by the caller.
    virtual void InfoBubbleFocusChanged() = 0;

   protected:
    virtual ~Delegate() {
    }
  };

  // Factory method to create new instances.
  // Creates the bubble, call |Show| to display it on screen.
  // |tab_contents| is the TabContents hosting the page.
  // |element_rect| is the display bounds of the html element requesting speech
  // input (in page coordinates).
  static SpeechInputBubble* Create(TabContents* tab_contents,
                                   Delegate* delegate,
                                   const gfx::Rect& element_rect);

  // This is implemented by platform specific code to create the underlying
  // bubble window. Not to be called directly by users of this class.
  static SpeechInputBubble* CreateNativeBubble(TabContents* tab_contents,
                                               Delegate* delegate,
                                               const gfx::Rect& element_rect);

  // |Create| uses the currently registered FactoryMethod to create the
  // SpeechInputBubble instances. FactoryMethod is intended for testing.
  typedef SpeechInputBubble* (*FactoryMethod)(TabContents*,
                                              Delegate*,
                                              const gfx::Rect&);
  // Sets the factory used by the static method Create. SpeechInputBubble does
  // not take ownership of |factory|. A value of NULL results in a
  // SpeechInputBubble being created directly.
#if defined(UNIT_TEST)
  static void set_factory(FactoryMethod factory) { factory_ = factory; }
#endif

  virtual ~SpeechInputBubble() {}

  // Indicates to the user that audio recording is in progress. If the bubble is
  // hidden, |Show| must be called to make it appear on screen.
  virtual void SetRecordingMode() = 0;

  // Indicates to the user that recognition is in progress. If the bubble is
  // hidden, |Show| must be called to make it appear on screen.
  virtual void SetRecognizingMode() = 0;

  // Displays the given string with the 'Try again' and 'Cancel' buttons. If the
  // bubble is hidden, |Show| must be called to make it appear on screen.
  virtual void SetMessage(const string16& text) = 0;

  // Brings up the bubble on screen.
  virtual void Show() = 0;

  // Hides the info bubble, resulting in a call to
  // |Delegate::InfoBubbleFocusChanged| as well.
  virtual void Hide() = 0;

  // Updates the current captured audio volume displayed on screen.
  virtual void SetInputVolume(float volume) = 0;

  // Returns the TabContents for which this bubble gets displayed.
  virtual TabContents* tab_contents() = 0;

  // The horizontal distance between the start of the html widget and the speech
  // bubble's arrow.
  static const int kBubbleTargetOffsetX;

 private:
  static FactoryMethod factory_;
};

// Base class for the platform specific bubble implementations, this contains
// the platform independent code for SpeechInputBubble.
class SpeechInputBubbleBase : public SpeechInputBubble {
 public:
  // The current display mode of the bubble, useful only for the platform
  // specific implementation.
  enum DisplayMode {
    DISPLAY_MODE_RECORDING,
    DISPLAY_MODE_RECOGNIZING,
    DISPLAY_MODE_MESSAGE
  };

  explicit SpeechInputBubbleBase(TabContents* tab_contents);
  virtual ~SpeechInputBubbleBase();

  // SpeechInputBubble methods
  virtual void SetRecordingMode();
  virtual void SetRecognizingMode();
  virtual void SetMessage(const string16& text);
  virtual void SetInputVolume(float volume);
  virtual TabContents* tab_contents();

 protected:
  // Updates the platform specific UI layout for the current display mode.
  virtual void UpdateLayout() = 0;

  // Sets the given image as the image to display in the speech bubble.
  // TODO(satish): Make the SetRecognizingMode call use this to show an
  // animation while waiting for results.
  virtual void SetImage(const SkBitmap& image) = 0;

  DisplayMode display_mode() {
    return display_mode_;
  }

  string16 message_text() {
    return message_text_;
  }

 private:
  void DoRecognizingAnimationStep();

  // Task factory used for animation timer.
  ScopedRunnableMethodFactory<SpeechInputBubbleBase> task_factory_;
  int animation_step_;  // Current index/step of the animation.
  std::vector<SkBitmap> animation_frames_;

  DisplayMode display_mode_;
  string16 message_text_;  // Text displayed in DISPLAY_MODE_MESSAGE
  // The current microphone image with volume level indication.
  scoped_ptr<SkBitmap> mic_image_;
  // A temporary buffer image used in creating the above mic image.
  scoped_ptr<SkBitmap> buffer_image_;
  // TabContents in which this this bubble gets displayed.
  TabContents* tab_contents_;

  static SkBitmap* mic_full_;  // Mic image with full volume.
  static SkBitmap* mic_empty_;  // Mic image with zero volume.
  static SkBitmap* mic_mask_;  // Gradient mask used by the volume indicator.
  static SkBitmap* spinner_;  // Spinner image for the progress animation.
  static const int kRecognizingAnimationStepMs;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechInputBubble::Delegate SpeechInputBubbleDelegate;

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_BUBBLE_H_
