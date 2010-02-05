// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"

namespace {
  const CGFloat kRightBorderWidth = 1.0;
  const CGFloat kRightBorderGrayscale = 0.5;
  const CGFloat kUpperPadding = 9.0;
  const CGFloat kLowerPadding = 5.0;
}  // namespace

@implementation BrowserActionsContainerView

@synthesize rightBorderShown = rightBorderShown_;

- (void)drawRect:(NSRect)dirtyRect {
  NSRect bounds = [self bounds];
  if (rightBorderShown_) {
    NSColor* middleColor =
        [NSColor colorWithCalibratedWhite:kRightBorderGrayscale alpha:1.0];
    NSColor* endPointColor =
        [NSColor colorWithCalibratedWhite:kRightBorderGrayscale alpha:0.0];
    NSGradient* borderGradient = [[[NSGradient alloc]
        initWithColorsAndLocations:endPointColor, (CGFloat)0.0,
                                   middleColor, (CGFloat)0.5,
                                   endPointColor, (CGFloat)1.0,
                                   nil] autorelease];
    CGFloat xPos = bounds.origin.x + bounds.size.width - kRightBorderWidth;
    NSRect borderRect = NSMakeRect(xPos, kLowerPadding, kRightBorderWidth,
        bounds.size.height - kUpperPadding);
    [borderGradient drawInRect:borderRect angle:90.0];
  }
}

@end
