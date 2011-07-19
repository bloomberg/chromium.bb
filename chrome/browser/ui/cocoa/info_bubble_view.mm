// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/info_bubble_view.h"

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"

@implementation InfoBubbleView

@synthesize arrowLocation = arrowLocation_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    arrowLocation_ = info_bubble::kTopLeft;
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  // Make room for the border to be seen.
  NSRect bounds = [self bounds];
  bounds.size.height -= info_bubble::kBubbleArrowHeight;
  NSBezierPath* bezier = [NSBezierPath bezierPath];
  rect.size.height -= info_bubble::kBubbleArrowHeight;

  // Start with a rounded rectangle.
  [bezier appendBezierPathWithRoundedRect:bounds
                                  xRadius:info_bubble::kBubbleCornerRadius
                                  yRadius:info_bubble::kBubbleCornerRadius];

  // Add the bubble arrow.
  CGFloat dX = 0;
  switch (arrowLocation_) {
    case info_bubble::kTopLeft:
      dX = info_bubble::kBubbleArrowXOffset;
      break;
    case info_bubble::kTopRight:
      dX = NSWidth(bounds) - info_bubble::kBubbleArrowXOffset -
          info_bubble::kBubbleArrowWidth;
      break;
    default:
      NOTREACHED();
      break;
  }
  NSPoint arrowStart = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
  arrowStart.x += dX;
  [bezier moveToPoint:NSMakePoint(arrowStart.x, arrowStart.y)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x +
                                      info_bubble::kBubbleArrowWidth / 2.0,
                                  arrowStart.y +
                                      info_bubble::kBubbleArrowHeight)];
  [bezier lineToPoint:NSMakePoint(arrowStart.x + info_bubble::kBubbleArrowWidth,
                                  arrowStart.y)];
  [bezier closePath];
  [[NSColor whiteColor] set];
  [bezier fill];
}

- (NSPoint)arrowTip {
  NSRect bounds = [self bounds];
  CGFloat tipXOffset =
      info_bubble::kBubbleArrowXOffset + info_bubble::kBubbleArrowWidth / 2.0;
  CGFloat xOffset =
      (arrowLocation_ == info_bubble::kTopRight) ? NSMaxX(bounds) - tipXOffset :
                                                   NSMinX(bounds) + tipXOffset;
  NSPoint arrowTip = NSMakePoint(xOffset, NSMaxY(bounds));
  return arrowTip;
}

@end
