// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_button.h"

@interface ToolbarButton (Private)
- (BOOL)updateStatus:(NSEvent*)theEvent;
@end

@implementation ToolbarButton

@synthesize handleMiddleClick = handleMiddleClick_;

- (void)mouseDown:(NSEvent*)theEvent {
  if (!handlingMiddleClick_)
    [super mouseDown:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  if (!handlingMiddleClick_)
    [super mouseDragged:theEvent];
}

- (void)mouseUp:(NSEvent*)theEvent {
  if (!handlingMiddleClick_)
    [super mouseUp:theEvent];
}

- (void)otherMouseDown:(NSEvent*)theEvent {
  if (![self shouldHandleEvent:theEvent])
    [super otherMouseDown:theEvent];
  else
    handlingMiddleClick_ = [self updateStatus:theEvent];
}

- (void)otherMouseDragged:(NSEvent*)theEvent {
  if (!handlingMiddleClick_ || ![self shouldHandleEvent:theEvent])
    [super otherMouseDragged:theEvent];
  else
    [self updateStatus:theEvent];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  if (!handlingMiddleClick_ || ![self shouldHandleEvent:theEvent]) {
    [super otherMouseUp:theEvent];
  } else {
    if ([self state] == NSOnState)
      [self sendAction:[self action] to:[self target]];

    [self setState:NSOffState];
    [self highlight:NO];
    handlingMiddleClick_ = NO;
  }
}

- (BOOL)updateStatus:(NSEvent*)theEvent {
  NSPoint mouseLoc = [self convertPoint:[theEvent locationInWindow]
                               fromView:nil];
  BOOL isInside = [self mouse:mouseLoc inRect:[self bounds]];
  [self setState:isInside ? NSOnState : NSOffState];
  [self highlight:isInside];
  return isInside;
}

- (BOOL)shouldHandleEvent:(NSEvent*)theEvent {
  // |buttonNumber| is the mouse button whose action triggered theEvent.
  // 2 corresponds to the middle mouse button.
  return handleMiddleClick_ && [theEvent buttonNumber] == 2;
}

@end
