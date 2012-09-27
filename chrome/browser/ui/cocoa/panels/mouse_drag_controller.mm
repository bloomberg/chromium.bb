// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/panels/mouse_drag_controller.h"

#include <Carbon/Carbon.h>  // kVK_Escape
#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"

// The distance the user has to move the mouse while keeping the left button
// down before panel resizing operation actually starts.
const double kDragThreshold = 3.0;

@implementation MouseDragController

- (NSView<MouseDragControllerClient>*)client {
  return client_;
}

- (NSPoint)initialMouseLocation {
  return initialMouseLocation_;
}

- (BOOL)exceedsDragThreshold:(NSPoint)mouseLocation {
  float deltaX = fabs(initialMouseLocation_.x - mouseLocation.x);
  float deltaY = fabs(initialMouseLocation_.y - mouseLocation.y);
  return deltaX > kDragThreshold || deltaY > kDragThreshold;
}

- (BOOL)tryStartDrag:(NSEvent*)event {
  DCHECK(dragState_ == PANEL_DRAG_CAN_START);
  NSPoint mouseLocation = [event locationInWindow];
  if (![self exceedsDragThreshold:mouseLocation])
    return NO;

  // Mouse moved over threshold, start drag.
  dragState_ = PANEL_DRAG_IN_PROGRESS;
  [client_ dragStarted:initialMouseLocation_];
  return YES;
}

- (void)cleanupAfterDrag {
  if (dragState_ == PANEL_DRAG_SUPPRESSED)
    return;
  dragState_ = PANEL_DRAG_SUPPRESSED;
  initialMouseLocation_ = NSZeroPoint;
  [client_ cleanupAfterDrag];
}

- (MouseDragController*)initWithClient:
      (NSView<MouseDragControllerClient>*)client {
  client_ = client;
  dragState_ = PANEL_DRAG_SUPPRESSED;
  return self;
}

- (void)mouseDown:(NSEvent*)event {
  DCHECK(dragState_ == PANEL_DRAG_SUPPRESSED);
  dragState_ = PANEL_DRAG_CAN_START;
  initialMouseLocation_ = [event locationInWindow];
  [client_ prepareForDrag];
}

- (void)mouseDragged:(NSEvent*)event {
  if (dragState_ == PANEL_DRAG_SUPPRESSED)
    return;

  // In addition to events needed to control the drag operation, fetch the right
  // mouse click events and key down events and ignore them, to prevent their
  // accumulation in the queue and "playing out" when the mouse is released.
  const NSUInteger mask =
      NSLeftMouseUpMask | NSLeftMouseDraggedMask | NSKeyUpMask |
      NSRightMouseDownMask | NSKeyDownMask ;

  while (true) {
    base::mac::ScopedNSAutoreleasePool autorelease_pool;
    BOOL keepGoing = YES;

    switch ([event type]) {
      case NSLeftMouseDragged: {
        // If drag didn't start yet, see if mouse moved far enough to start it.
        if (dragState_ == PANEL_DRAG_CAN_START && ![self tryStartDrag:event])
          return;

        DCHECK(dragState_ == PANEL_DRAG_IN_PROGRESS);
        [client_ dragProgress:[event locationInWindow]];
        break;
      }

      case NSKeyUp:
        if ([event keyCode] == kVK_Escape) {
          // The drag might not be started yet because of threshold, so check.
          if (dragState_ == PANEL_DRAG_IN_PROGRESS)
            [client_ dragEnded:YES];
          keepGoing = NO;
        }
        break;

      case NSLeftMouseUp:
        // The drag might not be started yet because of threshold, so check.
        if (dragState_ == PANEL_DRAG_IN_PROGRESS)
          [client_ dragEnded:NO];
        keepGoing = NO;
        break;

      case NSRightMouseDownMask:
        break;

      default:
        // Dequeue and ignore other mouse and key events so the Chrome context
        // menu does not come after right click on a page during Panel
        // resize, or the keystrokes are not 'accumulated' and entered
        // at once when the drag ends.
        break;
    }

    if (!keepGoing)
      break;

    autorelease_pool.Recycle();

    event = [NSApp nextEventMatchingMask:mask
                               untilDate:[NSDate distantFuture]
                                  inMode:NSDefaultRunLoopMode
                                 dequeue:YES];

  }
  [self cleanupAfterDrag];
}


- (void)mouseUp:(NSEvent*)event {
  if (dragState_ == PANEL_DRAG_SUPPRESSED)
    return;
  // The mouseUp while in drag should be processed by nested message loop
  // in mouseDragged: method.
  DCHECK(dragState_ != PANEL_DRAG_IN_PROGRESS);
  // Do cleanup in case the actual drag was not started (because of threshold).
  [self cleanupAfterDrag];
}
@end

