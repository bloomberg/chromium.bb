// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

@class ConfirmBubbleController;

// A view class that implements a bubble consisting of the following items:
// * one icon ("icon")
// * one title text ("title")
// * one message text ("message")
// * one optional link ("link")
// * two optional buttons ("ok" and "cancel")
//
// This bubble is convenient when we wish to ask transient, non-blocking
// questions. Unlike a dialog, a bubble menu disappears when we click outside of
// its window to avoid blocking user operations. A bubble is laid out as
// follows:
//
//   +------------------------+
//   | icon title             |
//   | message                |
//   | link                   |
//   |          [Cancel] [OK] |
//   +------------------------+
//
@interface ConfirmBubbleCocoa : NSView<NSTextViewDelegate> {
 @private
  NSView* parent_;  // weak
  ConfirmBubbleController* controller_;  // weak

  // Controls used in this bubble.
  base::scoped_nsobject<NSImageView> icon_;
  base::scoped_nsobject<NSTextView> titleLabel_;
  base::scoped_nsobject<NSTextView> messageLabel_;
  base::scoped_nsobject<NSButton> okButton_;
  base::scoped_nsobject<NSButton> cancelButton_;
}

// Initializes a bubble view. Since this initializer programmatically creates a
// custom NSView (i.e. without using a nib file), this function should be called
// from loadView: of the controller object which owns this view.
- (id)initWithParent:(NSView*)parent
          controller:(ConfirmBubbleController*)controller;

@end

// Exposed only for unit testing.
@interface ConfirmBubbleCocoa (ExposedForUnitTesting)
- (void)clickOk;
- (void)clickCancel;
- (void)clickLink;
@end

#endif  // CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_COCOA_H_
