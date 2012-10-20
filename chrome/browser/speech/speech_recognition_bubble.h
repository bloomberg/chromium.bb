// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia.h"

class SkBitmap;
class SkCanvas;

namespace content {
class WebContents;
}

namespace gfx {
class Canvas;
class Rect;
}

// SpeechRecognitionBubble displays a popup info bubble during speech
// recognition, points to the html element which requested speech recognition
// and shows progress events. The popup is closed by the user clicking anywhere
// outside the popup window, or by the caller destroying this object.
class SpeechRecognitionBubble {
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
  // |web_contents| is the WebContents hosting the page.
  // |element_rect| is the display bounds of the html element requesting speech
  // recognition (in page coordinates).
  static SpeechRecognitionBubble* Create(content::WebContents* web_contents,
                                         Delegate* delegate,
                                         const gfx::Rect& element_rect);

  // This is implemented by platform specific code to create the underlying
  // bubble window. Not to be called directly by users of this class.
  static SpeechRecognitionBubble* CreateNativeBubble(
      content::WebContents* web_contents,
      Delegate* delegate,
      const gfx::Rect& element_rect);

  // |Create| uses the currently registered FactoryMethod to create the
  // SpeechRecognitionBubble instances. FactoryMethod is intended for testing.
  typedef SpeechRecognitionBubble* (*FactoryMethod)(content::WebContents*,
                                                    Delegate*,
                                                    const gfx::Rect&);
  // Sets the factory used by the static method Create. SpeechRecognitionBubble
  // does not take ownership of |factory|. A value of NULL results in a
  // SpeechRecognitionBubble being created directly.
#if defined(UNIT_TEST)
  static void set_factory(FactoryMethod factory) { factory_ = factory; }
#endif

  virtual ~SpeechRecognitionBubble() {}

  // Indicates to the user that audio hardware is initializing. If the bubble is
  // hidden, |Show| must be called to make it appear on screen.
  virtual void SetWarmUpMode() = 0;

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

  // Updates and draws the current captured audio volume displayed on screen.
  virtual void SetInputVolume(float volume, float noise_volume) = 0;

  // Returns the WebContents for which this bubble gets displayed.
  virtual content::WebContents* GetWebContents() = 0;

  // The horizontal distance between the start of the html widget and the speech
  // bubble's arrow.
  static const int kBubbleTargetOffsetX;

 private:
  static FactoryMethod factory_;
};

// Base class for the platform specific bubble implementations, this contains
// the platform independent code for SpeechRecognitionBubble.
class SpeechRecognitionBubbleBase : public SpeechRecognitionBubble {
 public:
  // The current display mode of the bubble, useful only for the platform
  // specific implementation.
  enum DisplayMode {
    DISPLAY_MODE_WARM_UP,
    DISPLAY_MODE_RECORDING,
    DISPLAY_MODE_RECOGNIZING,
    DISPLAY_MODE_MESSAGE
  };

  explicit SpeechRecognitionBubbleBase(content::WebContents* web_contents);
  virtual ~SpeechRecognitionBubbleBase();

  // SpeechRecognitionBubble methods
  virtual void SetWarmUpMode() OVERRIDE;
  virtual void SetRecordingMode() OVERRIDE;
  virtual void SetRecognizingMode() OVERRIDE;
  virtual void SetMessage(const string16& text) OVERRIDE;
  virtual void SetInputVolume(float volume, float noise_volume) OVERRIDE;
  virtual content::WebContents* GetWebContents() OVERRIDE;

 protected:
  // Updates the platform specific UI layout for the current display mode.
  virtual void UpdateLayout() = 0;

  // Overridden by subclasses to copy |icon_image()| to the screen.
  virtual void UpdateImage() = 0;

  DisplayMode display_mode() const { return display_mode_; }

  const string16& message_text() const { return message_text_; }

  gfx::ImageSkia icon_image();

 private:
  void DoRecognizingAnimationStep();
  void DoWarmingUpAnimationStep();
  void SetImage(const gfx::ImageSkia& image);

  void DrawVolumeOverlay(SkCanvas* canvas,
                         const gfx::ImageSkia& image_skia,
                         float volume);

  // Task factory used for animation timer.
  base::WeakPtrFactory<SpeechRecognitionBubbleBase> weak_factory_;
  int animation_step_;  // Current index/step of the animation.

  DisplayMode display_mode_;
  string16 message_text_;  // Text displayed in DISPLAY_MODE_MESSAGE

  // The current microphone image with volume level indication.
  scoped_ptr<SkBitmap> mic_image_;
  // A temporary buffer image used in creating the above mic image.
  scoped_ptr<SkBitmap> buffer_image_;
  // WebContents in which this this bubble gets displayed.
  content::WebContents* web_contents_;
  // The current image displayed in the bubble's icon widget.
  gfx::ImageSkia icon_image_;
  // The scale factor used for the web-contents.
  ui::ScaleFactor scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionBubbleBase);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognitionBubble::Delegate SpeechRecognitionBubbleDelegate;

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_BUBBLE_H_
