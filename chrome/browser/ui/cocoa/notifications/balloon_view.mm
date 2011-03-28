// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/balloon_view.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/notifications/balloon_controller.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"

namespace {

const int kRoundedCornerSize = 6;

}  // namespace

@implementation BalloonWindow
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(unsigned int)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  self = [super initWithContentRect:contentRect
                          styleMask:NSBorderlessWindowMask
                            backing:NSBackingStoreBuffered
                              defer:NO];
  if (self) {
    [self setLevel:NSStatusWindowLevel];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
  }
  return self;
}

- (BOOL)canBecomeMainWindow {
  return NO;
}
@end

@implementation BalloonShelfViewCocoa
- (void)drawRect:(NSRect)rect {
  NSBezierPath* path =
      [NSBezierPath gtm_bezierPathWithRoundRect:[self bounds]
                            topLeftCornerRadius:kRoundedCornerSize
                           topRightCornerRadius:kRoundedCornerSize
                         bottomLeftCornerRadius:0.0
                        bottomRightCornerRadius:0.0];

  [[NSColor colorWithCalibratedWhite:0.957 alpha:1.0] set];
  [path fill];

  [[NSColor colorWithCalibratedWhite:0.8 alpha:1.0] set];
  NSPoint origin = [self bounds].origin;
  [NSBezierPath strokeLineFromPoint:origin
      toPoint:NSMakePoint(origin.x + NSWidth([self bounds]), origin.y)];
}
@end

@implementation BalloonContentViewCocoa
- (void)drawRect:(NSRect)rect {
  rect = NSInsetRect([self bounds], 0.5, 0.5);
  NSBezierPath* path =
      [NSBezierPath gtm_bezierPathWithRoundRect:rect
                            topLeftCornerRadius:0.0
                           topRightCornerRadius:0.0
                         bottomLeftCornerRadius:kRoundedCornerSize
                        bottomRightCornerRadius:kRoundedCornerSize];
  [[NSColor whiteColor] set];
  [path setLineWidth:3];
  [path stroke];
}
@end

@implementation BalloonOverlayViewCocoa

// We do not want to bring chrome window to foreground when we click on any
// part of the notification balloon. To do this, we first postpone the window
// reorder here (shouldDelayWindowOrderingForEvent is called during mouseDown)
// and then complete canceling the reorder by [NSApp preventWindowOrdering] in
// mouseDown handler of this view.
- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent*)theEvent {
  return YES;
}

- (void)mouseDown:(NSEvent*)event {
  [NSApp preventWindowOrdering];
  // Continue bubbling the event up the chain of responders.
  [super mouseDown:event];
}
@end
