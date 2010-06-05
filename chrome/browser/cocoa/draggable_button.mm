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
const CGFloat kDragExpirationTimeout = 1.0;

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

// Determine whether a mouse down should turn into a drag; started as copy of
// NSTableView code.
- (BOOL)dragShouldBeginFromMouseDown:(NSEvent*)mouseDownEvent
                      withExpiration:(NSDate*)expiration
                         xHysteresis:(float)xHysteresis
                         yHysteresis:(float)yHysteresis {
  if ([mouseDownEvent type] != NSLeftMouseDown) {
    return NO;
  }

  NSEvent* nextEvent = nil;
  NSEvent* firstEvent = nil;
  NSEvent* dragEvent = nil;
  NSEvent* mouseUp = nil;
  BOOL dragIt = NO;

  while ((nextEvent = [[self window]
      nextEventMatchingMask:(NSLeftMouseUpMask | NSLeftMouseDraggedMask)
                  untilDate:expiration
                     inMode:NSEventTrackingRunLoopMode
                    dequeue:YES]) != nil) {
    if (firstEvent == nil) {
      firstEvent = nextEvent;
    }
    if ([nextEvent type] == NSLeftMouseDragged) {
      float deltax = ABS([nextEvent locationInWindow].x -
          [mouseDownEvent locationInWindow].x);
      float deltay = ABS([nextEvent locationInWindow].y -
          [mouseDownEvent locationInWindow].y);
      dragEvent = nextEvent;
      if (deltax >= xHysteresis) {
        dragIt = YES;
        break;
      }
      if (deltay >= yHysteresis) {
        dragIt = YES;
        break;
      }
    } else if ([nextEvent type] == NSLeftMouseUp) {
      mouseUp = nextEvent;
      break;
    }
  }

  // Since we've been dequeuing the events (If we don't, we'll never see
  // the mouse up...), we need to push some of the events back on.
  // It makes sense to put the first and last drag events and the mouse
  // up if there was one.
  if (mouseUp != nil) {
    [NSApp postEvent:mouseUp atStart:YES];
  }
  if (dragEvent != nil) {
    [NSApp postEvent:dragEvent atStart:YES];
  }
  if (firstEvent != mouseUp && firstEvent != dragEvent) {
    [NSApp postEvent:firstEvent atStart:YES];
  }

  return dragIt;
}

- (BOOL)dragShouldBeginFromMouseDown:(NSEvent*)mouseDownEvent
                      withExpiration:(NSDate*)expiration {
  return [self dragShouldBeginFromMouseDown:mouseDownEvent
                             withExpiration:expiration
                                xHysteresis:kWebDragStartHysteresisX
                                yHysteresis:kWebDragStartHysteresisY];
}

- (void)mouseUp:(NSEvent*)theEvent {
  if (!draggable_) {
    [super mouseUp:theEvent];
    return;
  }

  // There are non-drag cases where a mouseUp: may happen
  // (e.g. mouse-down, cmd-tab to another application, move mouse,
  // mouse-up).  So we check.
  NSPoint viewLocal = [self convertPoint:[theEvent locationInWindow]
                                fromView:[[self window] contentView]];
  if (NSPointInRect(viewLocal, [self bounds])) {
    [self performClick:self];
  }
}

// Mimic "begin a click" operation visually.  Do NOT follow through
// with normal button event handling.
- (void)mouseDown:(NSEvent*)theEvent {
  if (draggable_) {
    [[self cell] setHighlighted:YES];
    NSDate* date = [NSDate dateWithTimeIntervalSinceNow:kDragExpirationTimeout];
    if ([self dragShouldBeginFromMouseDown:theEvent
                            withExpiration:date]) {
      [self beginDrag:theEvent];
      [self endDrag];
    } else {
      [super mouseDown:theEvent];
    }
  } else {
    [super mouseDown:theEvent];
  }
}

- (void)beginDrag:(NSEvent*)dragEvent {
  // Must be overridden by subclasses.
  NOTREACHED();
}

- (void)endDrag {
  [[self cell] setHighlighted:NO];
}

@end  // @interface DraggableButton
