// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"

namespace {
  const CGFloat kGrippyLowerPadding = 4.0;
  const CGFloat kGrippyUpperPadding = 8.0;
  const CGFloat kRightBorderXOffset = -1.0;
  const CGFloat kRightBorderWidth = 1.0;
  const CGFloat kRightBorderGrayscale = 0.5;
  const CGFloat kUpperPadding = 9.0;
  const CGFloat kLowerPadding = 5.0;
}  // namespace

@interface BrowserActionsContainerView(Private)
- (void)drawLeftGrippers;
@end

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
    CGFloat xPos = bounds.origin.x + bounds.size.width - kRightBorderWidth +
        kRightBorderXOffset;
    NSRect borderRect = NSMakeRect(xPos, kLowerPadding, kRightBorderWidth,
        bounds.size.height - kUpperPadding);
    [borderGradient drawInRect:borderRect angle:90.0];
  }

  [self drawLeftGrippers];
}

- (void)drawLeftGrippers {
  NSRect grippyRect = NSMakeRect(0.0, kLowerPadding + kGrippyLowerPadding, 1.0,
      [self bounds].size.height - kUpperPadding - kGrippyUpperPadding);
  [[NSColor colorWithCalibratedWhite:0.7 alpha:0.5] set];
  NSRectFill(grippyRect);

  [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] set];
  grippyRect.origin.x += 1.0;
  NSRectFill(grippyRect);

  grippyRect.origin.x += 1.0;

  [[NSColor colorWithCalibratedWhite:0.7 alpha:0.5] set];
  grippyRect.origin.x += 1.0;
  NSRectFill(grippyRect);

  [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] set];
  grippyRect.origin.x += 1.0;
  NSRectFill(grippyRect);
}

- (BrowserActionButton*)buttonAtIndex:(NSUInteger)index {
  return [[self subviews] objectAtIndex:index];
}

@end
