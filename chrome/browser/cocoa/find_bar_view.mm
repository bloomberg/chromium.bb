// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/find_bar_view.h"

namespace {
CGFloat kCurveSize = 8;
}  // end namespace

@implementation FindBarView

- (void)drawRect:(NSRect)rect {
  // TODO(rohitrao): Make this prettier.
  rect = NSInsetRect([self bounds], 0.5, 0.5);
  rect = NSOffsetRect(rect, 0, 1.0);

  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  NSPoint midLeft1 =
      NSMakePoint(NSMinX(rect) + kCurveSize, NSMaxY(rect) - kCurveSize);
  NSPoint midLeft2 =
      NSMakePoint(NSMinX(rect) + kCurveSize, NSMinY(rect) + kCurveSize);
  NSPoint midRight1 =
      NSMakePoint(NSMaxX(rect) - kCurveSize, NSMinY(rect) + kCurveSize);
  NSPoint midRight2 =
      NSMakePoint(NSMaxX(rect) - kCurveSize, NSMaxY(rect) - kCurveSize);
  NSPoint bottomLeft =
      NSMakePoint(NSMinX(rect) + (2 * kCurveSize), NSMinY(rect));
  NSPoint bottomRight =
      NSMakePoint(NSMaxX(rect) - (2 * kCurveSize), NSMinY(rect));

  NSBezierPath* path = [NSBezierPath bezierPath];
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
  NSGraphicsContext* context = [NSGraphicsContext currentContext];
  [context saveGraphicsState];
  [path addClip];

  // Set the pattern phase
  NSPoint phase = [self gtm_themePatternPhase];

  [context setPatternPhase:phase];
  [super drawBackground];
  [context restoreGraphicsState];

  [[self strokeColor] set];
  [path stroke];
}

// The findbar is mostly opaque, but has an 8px transparent border on the left
// and right sides (see |kCurveSize|).  This is an artifact of the way it is
// drawn.  We override hitTest to return nil for points in this transparent
// area.
- (NSView*)hitTest:(NSPoint)point {
  NSView* hitView = [super hitTest:point];
  if (hitView == self) {
    // |rect| is approximately equivalent to the opaque area of the findbar.
    NSRect rect = NSInsetRect([self bounds], kCurveSize, 0);
    if (!NSMouseInRect(point, rect, [self isFlipped]))
      return nil;
  }

  return hitView;
}

// Eat all mouse events, to prevent clicks from falling through to views below.
- (void)mouseDown:(NSEvent *)theEvent {
}

- (void)rightMouseDown:(NSEvent *)theEvent {
}

- (void)otherMouseDown:(NSEvent *)theEvent {
}

- (void)mouseUp:(NSEvent *)theEvent {
}

- (void)rightMouseUp:(NSEvent *)theEvent {
}

- (void)otherMouseUp:(NSEvent *)theEvent {
}

- (void)mouseMoved:(NSEvent *)theEvent {
}

- (void)mouseDragged:(NSEvent *)theEvent {
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
}
@end
