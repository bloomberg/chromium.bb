// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SPEECH_INPUT_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_SPEECH_INPUT_WINDOW_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "chrome/browser/speech/speech_input_bubble.h"
#include "chrome/browser/ui/cocoa/base_bubble_controller.h"

// Controller for the speech input bubble window. This bubble window gets
// displayed when the user starts speech input in a html input element.
@interface SpeechInputWindowController : BaseBubbleController {
 @private
  SpeechInputBubble::Delegate* delegate_;  // weak.
  SpeechInputBubbleBase::DisplayMode displayMode_;

  // References below are weak, being obtained from the nib.
  IBOutlet NSImageView* iconImage_;
  IBOutlet NSTextField* instructionLabel_;
  IBOutlet NSButton* cancelButton_;
  IBOutlet NSButton* tryAgainButton_;
  IBOutlet NSButton* micSettingsButton_;
}

// Initialize the window. |anchoredAt| is in screen coordinates.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                  delegate:(SpeechInputBubbleDelegate*)delegate
                anchoredAt:(NSPoint)anchoredAt;

// Handler for the cancel button.
- (IBAction)cancel:(id)sender;

// Handler for the try again button.
- (IBAction)tryAgain:(id)sender;

// Handler for the mic settings button.
- (IBAction)micSettings:(id)sender;

// Updates the UI with data related to the given display mode.
- (void)updateLayout:(SpeechInputBubbleBase::DisplayMode)mode
         messageText:(const string16&)messageText
           iconImage:(NSImage*)iconImage;

// Makes the speech input bubble visible on screen.
- (void)show;

// Hides the speech input bubble away from screen. This does NOT release the
// controller and the window.
- (void)hide;

// Sets the image to be displayed in the bubble's status ImageView. A future
// call to updateLayout may change the image.
// TODO(satish): Clean that up and move it into the platform independent
// SpeechInputBubbleBase class.
- (void)setImage:(NSImage*)image;

@end

#endif  // CHROME_BROWSER_UI_COCOA_SPEECH_INPUT_WINDOW_CONTROLLER_H_
