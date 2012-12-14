// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"

#include "base/logging.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"

@implementation WebIntentViewController

+ (NSRect)minimumInnerFrame {
  NSRect bounds = NSMakeRect(0, 0, WebIntentPicker::kWindowMinWidth,
                             WebIntentPicker::kWindowMinHeight);
  bounds = NSInsetRect(bounds,
                       chrome_style::kHorizontalPadding,
                       0);
  bounds.origin.y += chrome_style::kClientTopPadding;
  bounds.size.height = bounds.size.height -
      chrome_style::kClientTopPadding -
      chrome_style::kClientBottomPadding;
  return bounds;
}

- (void)sizeToFitAndLayout {
  NSRect innerFrame = [WebIntentViewController minimumInnerFrame];
  NSSize minSize = [self minimumSizeForInnerWidth:NSWidth(innerFrame)];
  innerFrame.size.width = std::max(minSize.width, NSWidth(innerFrame));
  innerFrame.size.width = std::min(
      static_cast<CGFloat>(WebIntentPicker::kWindowMaxWidth),
      NSWidth(innerFrame));
  innerFrame.size.height = std::max(minSize.height, NSHeight(innerFrame));

  NSRect bounds = NSInsetRect(innerFrame,
                              -chrome_style::kHorizontalPadding,
                              0);
  bounds.origin.y -= chrome_style::kClientTopPadding;
  bounds.size.height = NSHeight(innerFrame) +
      chrome_style::kClientTopPadding +
      chrome_style::kClientBottomPadding;

  [[self view] setFrameSize:bounds.size];
  [self layoutSubviewsWithinFrame:innerFrame];
}

- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth {
  NOTREACHED();
  return NSZeroSize;
}

- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame {
  NOTREACHED();
}

- (void)viewRemovedFromSuperview {
}

@end
