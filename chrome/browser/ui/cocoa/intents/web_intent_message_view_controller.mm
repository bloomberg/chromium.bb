// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_message_view_controller.h"

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@interface WebIntentMessageViewController ()
// Resize the title and message text field to fit the given width.
- (void)resizeTextFieldsToWidth:(CGFloat)width;
@end

@implementation WebIntentMessageViewController

- (id)init {
  if ((self = [super init])) {
    scoped_nsobject<NSView> view(
        [[FlippedView alloc] initWithFrame:NSZeroRect]);
    [self setView:view];

    titleTextField_.reset([constrained_window::CreateLabel() retain]);
    [[self view] addSubview:titleTextField_];
    messageTextField_.reset([constrained_window::CreateLabel() retain]);
    [[self view] addSubview:messageTextField_];
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  [titleTextField_ setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          title,
          chrome_style::kTitleFontStyle,
          NSNaturalTextAlignment,
          NSLineBreakByWordWrapping)];
}

- (void)setMessage:(NSString*)message {
  [messageTextField_ setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          message,
          chrome_style::kTextFontStyle,
          NSNaturalTextAlignment,
          NSLineBreakByWordWrapping)];
}

- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth {
  [self resizeTextFieldsToWidth:innerWidth];
  CGFloat height = NSHeight([titleTextField_ frame]);
  height += NSHeight([messageTextField_ frame]);
  height += chrome_style::kRowPadding;
  return NSMakeSize(innerWidth, height);
}

- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame {
  [self resizeTextFieldsToWidth:NSWidth(innerFrame)];

  NSRect titleFrame = [titleTextField_ frame];
  titleFrame.origin = innerFrame.origin;
  [titleTextField_ setFrame:titleFrame];

  NSRect messageFrame = [messageTextField_ frame];
  messageFrame.origin.x = NSMinX(innerFrame);
  messageFrame.origin.y = NSMaxY(titleFrame) +
      chrome_style::kRowPadding;
  [messageTextField_ setFrame:messageFrame];
}

- (void)resizeTextFieldsToWidth:(CGFloat)width {
  NSRect frame = NSZeroRect;

  frame.size.width = width;
  [messageTextField_ setFrame:frame];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
      messageTextField_];

  frame.size.width -= chrome_style::GetCloseButtonSize() +
                      WebIntentPicker::kIconTextPadding;
  [titleTextField_ setFrame:frame];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:titleTextField_];
}

@end
