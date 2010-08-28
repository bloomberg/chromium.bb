// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_SPEECH_INPUT_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_SPEECH_INPUT_WINDOW_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "chrome/browser/cocoa/base_bubble_controller.h"
#include "chrome/browser/speech/speech_input_bubble.h"

// Controller for the speech input bubble window. This bubble window gets
// displayed when the user starts speech input in a html input element.
@interface SpeechInputWindowController : BaseBubbleController {
 @private
  SpeechInputBubble::Delegate* delegate_;  // weak.

  // References below are weak, being obtained from the nib.
  IBOutlet NSImageView* iconImage_;
  IBOutlet NSTextField* instructionLabel_;
  IBOutlet NSButton* cancelButton_;
}

// Initialize the window. |anchoredAt| is in screen coordinates.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                  delegate:(SpeechInputBubbleDelegate*)delegate
                anchoredAt:(NSPoint)anchoredAt;

// Handler for the cancel button.
- (IBAction)cancel:(id)sender;

// Inform the user that audio recording has ended and recognition is underway.
- (void)didStartRecognition;

@end

#endif  // CHROME_BROWSER_COCOA_SPEECH_INPUT_WINDOW_CONTROLLER_H_
