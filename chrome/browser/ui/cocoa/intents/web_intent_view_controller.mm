// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"

#include "base/logging.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/constrained_window_constants.h"

@implementation WebIntentViewController

+ (NSRect)minimumInnerFrame {
  NSRect bounds = NSMakeRect(0, 0, WebIntentPicker::kWindowMinWidth,
                             WebIntentPicker::kWindowMinHeight);
  bounds = NSInsetRect(bounds,
                       ConstrainedWindowConstants::kHorizontalPadding,
                       0);
  bounds.origin.y += ConstrainedWindowConstants::kClientTopPadding;
  bounds.size.height = bounds.size.height -
      ConstrainedWindowConstants::kClientTopPadding -
      ConstrainedWindowConstants::kClientBottomPadding;
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
                              -ConstrainedWindowConstants::kHorizontalPadding,
                              0);
  bounds.origin.y -= ConstrainedWindowConstants::kClientTopPadding;
  bounds.size.height = NSHeight(innerFrame) +
      ConstrainedWindowConstants::kClientTopPadding +
      ConstrainedWindowConstants::kClientBottomPadding;

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

@end
