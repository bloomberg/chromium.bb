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
  NSPoint midLeft1 =
      NSMakePoint(NSMinX(rect) + curveSize, NSMaxY(rect) - curveSize);
  NSPoint midLeft2 =
      NSMakePoint(NSMinX(rect) + curveSize, NSMinY(rect) + curveSize);
  NSPoint midRight1 =
      NSMakePoint(NSMaxX(rect) - curveSize, NSMinY(rect) + curveSize);
  NSPoint midRight2 =
      NSMakePoint(NSMaxX(rect) - curveSize, NSMaxY(rect) - curveSize);
  NSPoint bottomLeft =
      NSMakePoint(NSMinX(rect) + (2 * curveSize), NSMinY(rect));
  NSPoint bottomRight =
      NSMakePoint(NSMaxX(rect) - (2 * curveSize), NSMinY(rect));

  NSBezierPath *path = [NSBezierPath bezierPath];
  [path moveToPoint:topLeft];
  [path curveToPoint:midLeft1
        controlPoint1:NSMakePoint(midLeft1.x, topLeft.y)
        controlPoint2:NSMakePoint(midLeft1.x, topLeft.y)];
  [path lineToPoint:midLeft2];
  [path curveToPoint:bottomLeft
        controlPoint1:NSMakePoint(midLeft2.x, bottomLeft.y)
        controlPoint2:NSMakePoint(midLeft2.x, bottomLeft.y)];

  [path lineToPoint:bottomRight];
  [path curveToPoint:midRight1
        controlPoint1:NSMakePoint(midRight1.x, bottomLeft.y)
        controlPoint2:NSMakePoint(midRight1.x, bottomLeft.y)];
  [path lineToPoint:midRight2];
  [path curveToPoint:topRight
        controlPoint1:NSMakePoint(midRight2.x, topLeft.y)
        controlPoint2:NSMakePoint(midRight2.x, topLeft.y)];

  [NSGraphicsContext saveGraphicsState];
  [path addClip];
  [super drawRect:rect];
  [NSGraphicsContext restoreGraphicsState];

  [[self strokeColor] set];
  [path stroke];
}

@end
