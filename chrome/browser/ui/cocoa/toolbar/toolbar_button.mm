// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_button.h"

@implementation ToolbarButton

@synthesize handleMiddleClick = handleMiddleClick_;

- (void)otherMouseDown:(NSEvent*)theEvent {
  if (![self shouldHandleEvent:theEvent]) {
    [super otherMouseDown:theEvent];
    return;
  }

  NSEvent* nextEvent = theEvent;
  BOOL isInside;

  // Loop until middle button is released. Also, the mouse cursor is outside of
  // the button, the button should not be highlighted.
  do {
    NSPoint mouseLoc = [self convertPoint:[nextEvent locationInWindow]
                                 fromView:nil];
    isInside = [self mouse:mouseLoc inRect:[self bounds]];
    [self highlight:isInside];
    [self setState:isInside ? NSOnState : NSOffState];

    NSUInteger mask = NSOtherMouseDraggedMask | NSOtherMouseUpMask;
    nextEvent = [[self window] nextEventMatchingMask:mask];
  } while (!([nextEvent buttonNumber] == 2 &&
             [nextEvent type] == NSOtherMouseUp));

  // Discard the events before the middle button up event.
  // If we don't discard it, the events will be re-processed later.
  [[self window] discardEventsMatchingMask:NSAnyEventMask
                               beforeEvent:nextEvent];

  [self highlight:NO];
  [self setState:NSOffState];
  if (isInside)
    [self sendAction:[self action] to:[self target]];
}

- (BOOL)shouldHandleEvent:(NSEvent*)theEvent {
  // |buttonNumber| is the mouse button whose action triggered theEvent.
  // 2 corresponds to the middle mouse button.
  return handleMiddleClick_ && [theEvent buttonNumber] == 2;
}

@end
