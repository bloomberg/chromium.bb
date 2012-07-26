// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

class ConfirmBubbleModel;

// A view controller that manages a bubble view and becomes a proxy between
// the view and the ConfirmBubbleModel object. This class is internally used
// in ShowConfirmBubble() and users do not have to change this class directly.
@interface ConfirmBubbleController :
    NSViewController<NSTextViewDelegate> {
 @private
  NSView* parent_;  // weak
  CGPoint origin_;
  scoped_ptr<ConfirmBubbleModel> model_;
}

// Creates a ConfirmBubbleController object. The ConfirmBubbleController
// controller takes the ownership of the passed-in ConfirmBubbleModel.
- (id)initWithParent:(NSView*)parent
              origin:(CGPoint)origin
               model:(ConfirmBubbleModel*)model;

// Access to the properties of the ConfirmBubbleModel object. These functions
// also converts C++ types returned by the ConfirmBubbleModel object to
// Objective-C types.
- (NSPoint)origin;
- (NSString*)title;
- (NSString*)messageText;
- (NSString*)linkText;
- (NSString*)okButtonText;
- (NSString*)cancelButtonText;
- (BOOL)hasOkButton;
- (BOOL)hasCancelButton;
- (NSImage*)icon;

// Handle actions from the ConfirmBubbleCocoa objet.
- (void)accept;
- (void)cancel;
- (void)linkClicked;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_CONTROLLER_H_
