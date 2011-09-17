// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_exit_bubble_view.h"

#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

CGFloat kCurveSize = 8;

@implementation FullscreenExitBubbleView

- (void)drawRect:(NSRect)rect {
  const CGFloat lineWidth = [self cr_lineWidth];

  rect = NSOffsetRect([self bounds], 0, lineWidth*2);

  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  NSPoint midLeft =
      NSMakePoint(NSMinX(rect), NSMinY(rect) + kCurveSize);
  NSPoint midRight =
      NSMakePoint(NSMaxX(rect), NSMinY(rect) + kCurveSize);
  NSPoint bottomLeft =
      NSMakePoint(NSMinX(rect) + kCurveSize, NSMinY(rect));
  NSPoint bottomRight =
      NSMakePoint(NSMaxX(rect) - kCurveSize, NSMinY(rect));

  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:topLeft];
  [path appendBezierPathWithArcWithCenter:NSMakePoint(bottomLeft.x, midLeft.y)
      radius:kCurveSize startAngle:180 endAngle:270];

  [path lineToPoint:bottomRight];
  [path appendBezierPathWithArcWithCenter:NSMakePoint(bottomRight.x, midRight.y)
      radius:kCurveSize startAngle:270 endAngle:360];
  [path lineToPoint:topRight];

  {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    [path addClip];

    const NSRect bounds = [self bounds];

    [[NSColor colorWithDeviceWhite:0 alpha:0.7] set];
    NSRectFillUsingOperation(bounds, NSCompositeSourceOver);
  }

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

// Specifies that mouse events over this view should be ignored by the
// render host.
- (BOOL)nonWebContentView {
  return YES;
}

@end
