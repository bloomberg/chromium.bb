// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_progress_view_controller.h"

#include <cmath>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/spinner_progress_indicator.h"
#import "chrome/browser/ui/constrained_window.h"
#include "grit/theme_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Vertical space between the progress indicator and the message text field.
const CGFloat kProgressMessageFieldSpacing = 15.0;

// Joins the two strings with a space between them.
NSAttributedString* JoinString(NSAttributedString* string1,
                               NSAttributedString* string2) {
  if (![string1 length])
    return string2;
  if (![string2 length])
    return string1;

  NSMutableAttributedString* result =
      [[[NSMutableAttributedString alloc] init] autorelease];
  [result appendAttributedString:string1];
  scoped_nsobject<NSAttributedString> space(
      [[NSAttributedString alloc] initWithString:@" "]);
  [result appendAttributedString:space];
  [result appendAttributedString:string2];
  return result;
}

}  // namespace

@interface WebIntentProgressViewController ()
// Updates the message text field and resizes it to fit the given width.
- (void)updateTextFieldAndResizeToWidth:(CGFloat)width;
@end

@implementation WebIntentProgressViewController

- (id)init {
  if ((self = [super init])) {
    scoped_nsobject<NSView> view(
        [[FlippedView alloc] initWithFrame:NSZeroRect]);
    [self setView:view];

    messageTextField_.reset([constrained_window::CreateLabel() retain]);
    [[self view] addSubview:messageTextField_];

    progressIndicator_.reset(
        [[SpinnerProgressIndicator alloc] initWithFrame:NSZeroRect]);
    [progressIndicator_ sizeToFit];
    [[self view] addSubview:progressIndicator_];
  }
  return self;
}

- (SpinnerProgressIndicator*)progressIndicator {
  return progressIndicator_;
}

- (void)setTitle:(NSString*)title {
  title_.reset([title retain]);
}

- (void)setMessage:(NSString*)message {
  message_.reset([message retain]);
}

- (void)setPercentDone:(int)percent {
  if (percent == -1) {
    [progressIndicator_ setIsIndeterminate:YES];
  } else {
    [progressIndicator_ setIsIndeterminate:NO];
    [progressIndicator_ setPercentDone:percent];
  }
}

- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth {
  NSSize progressSize = [progressIndicator_ frame].size;
  CGFloat width = std::max(innerWidth, progressSize.width);
  [self updateTextFieldAndResizeToWidth:width];
  CGFloat height = progressSize.height + NSHeight([messageTextField_ frame]) +
                   kProgressMessageFieldSpacing;
  return NSMakeSize(width, height);
}

- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame {
  [self updateTextFieldAndResizeToWidth:NSWidth(innerFrame)];

  NSRect progressFrame = [progressIndicator_ frame];
  progressFrame.origin.x =
      roundf(NSMidX(innerFrame) - NSWidth(progressFrame) / 2.0);
  progressFrame.origin.y = roundf(NSMinY(innerFrame) +
      NSHeight(innerFrame) / 3.0 - NSHeight(progressFrame) / 2.0);

  NSRect textFrame = [messageTextField_ frame];
  CGFloat newHeight = NSMaxY(progressFrame) + NSHeight(textFrame) +
                      kProgressMessageFieldSpacing;
  if (newHeight > NSHeight(innerFrame))
    progressFrame.origin.y = NSMinY(innerFrame);
  [progressIndicator_ setFrame:progressFrame];

  textFrame.origin.x = NSMinX(innerFrame);
  textFrame.origin.y = NSMaxY(progressFrame) + kProgressMessageFieldSpacing;
  [messageTextField_ setFrame:textFrame];
}

- (void)updateTextFieldAndResizeToWidth:(CGFloat)width {
  NSAttributedString* title = constrained_window::GetAttributedLabelString(
      title_,
      chrome_style::kBoldTextFontStyle,
      NSCenterTextAlignment,
      NSLineBreakByWordWrapping);
  NSAttributedString* message = constrained_window::GetAttributedLabelString(
      message_,
      chrome_style::kTextFontStyle,
      NSCenterTextAlignment,
      NSLineBreakByWordWrapping);
  [messageTextField_ setAttributedStringValue:JoinString(title, message)];

  NSRect frame = NSZeroRect;
  frame.size.width = width;
  [messageTextField_ setFrame:frame];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
      messageTextField_];
}

@end
