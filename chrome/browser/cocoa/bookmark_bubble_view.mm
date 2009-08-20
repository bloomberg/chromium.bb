// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bubble_view.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

namespace {
// TODO(jrg): confirm constants with UI dudes
const CGFloat kBubbleCornerRadius = 8.0;
const CGFloat kBubbleArrowXOffset = 10.0;
const CGFloat kBubbleArrowWidth = 15.0;
const CGFloat kBubbleArrowHeight = 8.0;
const CGFloat kBubbleBorderLineWidth = 1.0;
}

@implementation BookmarkBubbleView

- (void)drawRect:(NSRect)rect {
  // Make room for the border to be seen.
  NSRect bounds = [self bounds];
  bounds.size.height -= kBubbleArrowHeight;
  bounds = NSInsetRect(bounds,
                       kBubbleBorderLineWidth/2.0,
                       kBubbleBorderLineWidth/2.0);

  NSBezierPath* bezier = [NSBezierPath bezierPath];
  rect.size.height -= kBubbleArrowHeight;

  // Start with a rounded rectangle.
  [bezier appendBezierPathWithRoundedRect:bounds
                                  xRadius:kBubbleCornerRadius
                                  yRadius:kBubbleCornerRadius];

  // Add the bubble arrow (pointed at the star).
  NSPoint arrowStart = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
  arrowStart.x += kBubbleArrowXOffset;
  [bezier moveToPoint:NSMakePoint(arrowStart.x, arrowStart.y)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth/2.0,
                                  arrowStart.y + kBubbleArrowHeight)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth,
                                  arrowStart.y)];
  [bezier closePath];

  // Draw the outline...
  [[NSColor blackColor] set];
  [bezier setLineWidth:kBubbleBorderLineWidth];
  [bezier stroke];

  // Then fill the inside.
  GTMTheme *theme = [GTMTheme defaultTheme];
  NSGradient *gradient = [theme gradientForStyle:GTMThemeStyleToolBar
                                           state:NO];
  [gradient drawInBezierPath:bezier angle:0.0];
}

@end

