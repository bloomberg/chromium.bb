// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/draggable_button.h"

#include "base/logging.h"
#import "base/scoped_nsobject.h"

namespace {

// Code taken from <http://codereview.chromium.org/180036/diff/3001/3004>.
// TODO(viettrungluu): Do we want common, standard code for drag hysteresis?
const CGFloat kWebDragStartHysteresisX = 5.0;
const CGFloat kWebDragStartHysteresisY = 5.0;

}

@implementation DraggableButton

@synthesize draggable = draggable_;

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    draggable_ = YES;
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)coder {
  if ((self = [super initWithCoder:coder])) {
    draggable_ = YES;
  }
  return self;
}

- (void)mouseUp:(NSEvent*)theEvent {
  // Make sure that we can't start a drag until we see a mouse down again.
  mayDragStart_ = NO;

  // This conditional is never true (DnD loops in Cocoa eat the mouse
  // up) but I added it in case future versions of Cocoa do unexpected
  // things.
  if (beingDragged_) {
    NOTREACHED();
    return [super mouseUp:theEvent];
  }

  // There are non-drag cases where a mouseUp: may happen
  // (e.g. mouse-down, cmd-tab to another application, move mouse,
  // mouse-up).  So we check.
  NSPoint viewLocal = [self convertPoint:[theEvent locationInWindow]
                                fromView:[[self window] contentView]];
  if (NSPointInRect(viewLocal, [self bounds])) {
    [self performClick:self];
  } else {
    [self endDrag];
  }
}

// Mimic "begin a click" operation visually.  Do NOT follow through
// with normal button event handling.
- (void)mouseDown:(NSEvent*)theEvent {
  mayDragStart_ = YES;
  [[self cell] setHighlighted:YES];
  initialMouseDownLocation_ = [theEvent locationInWindow];
}

// Return YES if we have crossed a threshold of movement after
// mouse-down when we should begin a drag.  Else NO.
- (BOOL)hasCrossedDragThreshold:(NSEvent*)theEvent {
  NSPoint currentLocation = [theEvent locationInWindow];

  if ((abs(currentLocation.x - initialMouseDownLocation_.x) >
       kWebDragStartHysteresisX) ||
      (abs(currentLocation.y - initialMouseDownLocation_.y) >
       kWebDragStartHysteresisY)) {
    return YES;
  } else {
    return NO;
  }
}

- (void)mouseDragged:(NSEvent*)theEvent {
  if (beingDragged_) {
    [super mouseDragged:theEvent];
  } else if (draggable_ && mayDragStart_ &&
             [self hasCrossedDragThreshold:theEvent]) {
    // Starting drag. Never start another drag until another mouse down.
    mayDragStart_ = NO;
    [self beginDrag:theEvent];
  }
}

- (void)beginDrag:(NSEvent*)dragEvent {
  // Must be overridden by subclasses.
  NOTREACHED();
}

- (void)endDrag {
  beingDragged_ = NO;
  [[self cell] setHighlighted:NO];
}

@end
