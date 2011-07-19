// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/base_view.h"

NSString* kViewDidBecomeFirstResponder =
    @"Chromium.kViewDidBecomeFirstResponder";
NSString* kSelectionDirection = @"Chromium.kSelectionDirection";

@implementation BaseView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    trackingArea_ =
        [[NSTrackingArea alloc] initWithRect:frame
                                     options:NSTrackingMouseMoved |
                                             NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveInActiveApp |
                                             NSTrackingInVisibleRect
                                       owner:self
                                    userInfo:nil];
    [self addTrackingArea:trackingArea_];
  }
  return self;
}

- (void)dealloc {
  [self removeTrackingArea:trackingArea_];
  [trackingArea_ release];

  [super dealloc];
}

- (void)mouseEvent:(NSEvent *)theEvent {
  // This method left intentionally blank.
}

- (void)keyEvent:(NSEvent *)theEvent {
  // This method left intentionally blank.
}

- (void)mouseDown:(NSEvent *)theEvent {
  dragging_ = YES;
  [self mouseEvent:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];

  dragging_ = NO;
  if (pendingExitEvent_.get()) {
    NSEvent* exitEvent =
        [NSEvent enterExitEventWithType:NSMouseExited
                               location:[theEvent locationInWindow]
                          modifierFlags:[theEvent modifierFlags]
                              timestamp:[theEvent timestamp]
                           windowNumber:[theEvent windowNumber]
                                context:[theEvent context]
                            eventNumber:[pendingExitEvent_.get() eventNumber]
                         trackingNumber:[pendingExitEvent_.get() trackingNumber]
                               userData:[pendingExitEvent_.get() userData]];
    [self mouseEvent:exitEvent];
    pendingExitEvent_.reset();
  }
}

- (void)rightMouseUp:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseMoved:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseDragged:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseEntered:(NSEvent *)theEvent {
  if (pendingExitEvent_.get()) {
    pendingExitEvent_.reset();
    return;
  }

  [self mouseEvent:theEvent];
}

- (void)mouseExited:(NSEvent *)theEvent {
  // The tracking area will send an exit event even during a drag, which isn't
  // how the event flow for drags should work. This stores the exit event, and
  // sends it when the drag completes instead.
  if (dragging_) {
    pendingExitEvent_.reset([theEvent retain]);
    return;
  }

  [self mouseEvent:theEvent];
}

- (void)keyDown:(NSEvent *)theEvent {
  [self keyEvent:theEvent];
}

- (void)keyUp:(NSEvent *)theEvent {
  [self keyEvent:theEvent];
}

- (void)flagsChanged:(NSEvent *)theEvent {
  [self keyEvent:theEvent];
}

- (gfx::Rect)flipNSRectToRect:(NSRect)rect {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y([self bounds].size.height - new_rect.y() - new_rect.height());
  return new_rect;
}

- (NSRect)flipRectToNSRect:(gfx::Rect)rect {
  NSRect new_rect(NSRectFromCGRect(rect.ToCGRect()));
  new_rect.origin.y =
      [self bounds].size.height - new_rect.origin.y - new_rect.size.height;
  return new_rect;
}

@end
