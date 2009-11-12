// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/info_bubble_view.h"

#include "base/logging.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

namespace {
// TODO(andybons): confirm constants with UI dudes
const CGFloat kBubbleCornerRadius = 8.0;
const CGFloat kBubbleArrowXOffset = 10.0;
const CGFloat kBubbleArrowWidth = 15.0;
const CGFloat kBubbleArrowHeight = 8.0;
}

@implementation InfoBubbleView

@synthesize arrowLocation = arrowLocation_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    arrowLocation_ = kTopLeft;
  }

  return self;
}

- (void)drawRect:(NSRect)rect {
  // Make room for the border to be seen.
  NSRect bounds = [self bounds];
  bounds.size.height -= kBubbleArrowHeight;
  NSBezierPath* bezier = [NSBezierPath bezierPath];
  rect.size.height -= kBubbleArrowHeight;

  // Start with a rounded rectangle.
  [bezier appendBezierPathWithRoundedRect:bounds
                                  xRadius:kBubbleCornerRadius
                                  yRadius:kBubbleCornerRadius];

  // Add the bubble arrow.
  CGFloat dX = 0;
  switch (arrowLocation_) {
    case kTopLeft:
      dX = kBubbleArrowXOffset;
      break;
    case kTopRight:
      dX = NSWidth(bounds) - kBubbleArrowXOffset - kBubbleArrowWidth;
      break;
    default:
      NOTREACHED();
      break;
  }
  NSPoint arrowStart = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
  arrowStart.x += dX;
  [bezier moveToPoint:NSMakePoint(arrowStart.x, arrowStart.y)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth/2.0,
                                  arrowStart.y + kBubbleArrowHeight)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + kBubbleArrowWidth,
                                  arrowStart.y)];
  [bezier closePath];

  // Then fill the inside.
  GTMTheme *theme = [GTMTheme defaultTheme];
  NSGradient *gradient = [theme gradientForStyle:GTMThemeStyleToolBar
                                           state:NO];
  [gradient drawInBezierPath:bezier angle:0.0];
}

@end
