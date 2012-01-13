// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"

class ConfirmBubbleModel;

// A view controller that manages a bubble view and becomes a proxy between
// the view and the ConfirmBubbleModel object. This class is internally used
// in ConfirmBubbleView::Show() and users do not have to change this class
// directly.
@interface ConfirmBubbleController :
    NSViewController<NSTextViewDelegate> {
 @private
  NSView* parent_;  // weak
  CGPoint origin_;
  ConfirmBubbleModel* model_;  // weak
}

// Creates a ConfirmBubbleController object.
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

// Handle actions from from the ConfirmBubbleView objet.
- (void)accept;
- (void)cancel;
- (void)linkClicked;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONFIRM_BUBBLE_CONTROLLER_H_
