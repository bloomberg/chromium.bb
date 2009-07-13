// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/find_bar_view.h"

@implementation FindBarView

- (void)drawRect:(NSRect)rect {
  CGFloat curveSize = 8;
  // TODO(rohitrao): Make this prettier.
  rect = NSInsetRect([self bounds], 0.5, 0.5);
  rect = NSOffsetRect(rect, 0, 1.0);

  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  // Inset the bottom points by 1 so we draw the border entirely
  // inside the frame.
  NSPoint bottomLeft = NSMakePoint(NSMinX(rect) + curveSize, NSMinY(rect) + 1);
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect) - curveSize, NSMinY(rect) + 1);

  NSBezierPath *path = [NSBezierPath bezierPath];
  [path moveToPoint:topLeft];
  [path curveToPoint:bottomLeft
        controlPoint1:NSMakePoint(topLeft.x + curveSize, topLeft.y)
        controlPoint2:NSMakePoint(bottomLeft.x - curveSize, bottomLeft.y)];
  [path lineToPoint:bottomRight];
  [path curveToPoint:topRight
        controlPoint1:NSMakePoint(bottomRight.x + curveSize, bottomRight.y)
        controlPoint2:NSMakePoint(topRight.x - curveSize, topRight.y)];

  [NSGraphicsContext saveGraphicsState];
  [path addClip];
  [super drawRect:rect];
  [NSGraphicsContext restoreGraphicsState];

  [[self strokeColor] set];
  [path stroke];
}

@end
