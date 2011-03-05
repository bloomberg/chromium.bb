// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/draggable_button.h"

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
@synthesize actsOnMouseDown = actsOnMouseDown_;
@synthesize durationMouseWasDown = durationMouseWasDown_;

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    draggable_ = YES;
    actsOnMouseDown_ = NO;
    actionHasFired_ = NO;
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)coder {
  if ((self = [super initWithCoder:coder])) {
    draggable_ = YES;
    actsOnMouseDown_ = NO;
    actionHasFired_ = NO;
  }
  return self;
}

- (BOOL)deltaIndicatesDragStartWithXDelta:(float)xDelta
                                   yDelta:(float)yDelta
                              xHysteresis:(float)xHysteresis
                              yHysteresis:(float)yHysteresis {
  return (ABS(xDelta) >= xHysteresis) || (ABS(yDelta) >= yHysteresis);
}

- (BOOL)deltaIndicatesConclusionReachedWithXDelta:(float)xDelta
                                           yDelta:(float)yDelta
                                      xHysteresis:(float)xHysteresis
                                      yHysteresis:(float)yHysteresis {
  return (ABS(xDelta) >= xHysteresis) || (ABS(yDelta) >= yHysteresis);
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
      float deltax = [nextEvent locationInWindow].x -
          [mouseDownEvent locationInWindow].x;
      float deltay = [nextEvent locationInWindow].y -
          [mouseDownEvent locationInWindow].y;
      dragEvent = nextEvent;
      if ([self deltaIndicatesConclusionReachedWithXDelta:deltax
                                                   yDelta:deltay
                                              xHysteresis:xHysteresis
                                              yHysteresis:yHysteresis]) {
        dragIt = [self deltaIndicatesDragStartWithXDelta:deltax
                                                  yDelta:deltay
                                             xHysteresis:xHysteresis
                                             yHysteresis:yHysteresis];
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
  durationMouseWasDown_ = [theEvent timestamp] - whenMouseDown_;

  if (actionHasFired_)
    return;

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

- (void)secondaryMouseUpAction:(BOOL)wasInside {
  // Override if you want to do any extra work on mouseUp, after a mouseDown
  // action has already fired.
}

- (BOOL)acceptsTrackInFrom:(id)sender {
  return (sender == self);
}

- (void)performMouseDownAction:(NSEvent*)theEvent {
  int eventMask = NSLeftMouseUpMask | NSMouseEnteredMask | NSMouseExitedMask;

  [[self target] performSelector:[self action] withObject:self];
  actionHasFired_ = YES;

  DraggableButton* insideBtn = nil;

  while (1) {
    theEvent = [[self window] nextEventMatchingMask:eventMask];
    NSPoint mouseLoc = [self convertPoint:[theEvent locationInWindow]
                                 fromView:nil];
    BOOL isInside = [self mouse:mouseLoc inRect:[self bounds]];

    switch ([theEvent type]) {
      case NSMouseEntered:
      case NSMouseExited: {
        NSView* trackedView = (NSView*)[[theEvent trackingArea] owner];
        if (trackedView && [trackedView isKindOfClass:[self class]]) {
          DraggableButton *btn = static_cast<DraggableButton*>(trackedView);
          if (![btn acceptsTrackInFrom:self])
            break;
          if ([theEvent type] == NSMouseEntered) {
            [[NSCursor arrowCursor] set];
            [[btn cell] mouseEntered:theEvent];
            insideBtn = btn;
          } else {
            [[btn cell] mouseExited:theEvent];
            if (insideBtn == btn)
              insideBtn = nil;
          }
        }
        break;
      }
      case NSLeftMouseUp: {
        if (!isInside && insideBtn && insideBtn != self) {
          // Has tracked onto another DraggableButton menu item, and released,
          // so click it.
          [insideBtn performClick:self];
        }
        durationMouseWasDown_ = [theEvent timestamp] - whenMouseDown_;
        [self secondaryMouseUpAction:isInside];
        [[self cell] mouseExited:theEvent];
        [[insideBtn cell] mouseExited:theEvent];
        return;
        break;
      }
      default:
        /* Ignore any other kind of event. */
        break;
    }
  }
}

// Mimic "begin a click" operation visually.  Do NOT follow through
// with normal button event handling.
- (void)mouseDown:(NSEvent*)theEvent {
  [[NSCursor arrowCursor] set];

  whenMouseDown_ = [theEvent timestamp];
  actionHasFired_ = NO;

  if (draggable_) {
    NSDate* date = [NSDate dateWithTimeIntervalSinceNow:kDragExpirationTimeout];
    if ([self dragShouldBeginFromMouseDown:theEvent
                            withExpiration:date]) {
      [self beginDrag:theEvent];
      [self endDrag];
    } else {
      if (actsOnMouseDown_) {
        [self performMouseDownAction:theEvent];
      } else {
        [super mouseDown:theEvent];
      }

    }
  } else {
    if (actsOnMouseDown_) {
      [self performMouseDownAction:theEvent];
    } else {
      [super mouseDown:theEvent];
    }
  }
}

- (void)beginDrag:(NSEvent*)dragEvent {
  // Must be overridden by subclasses.
  NOTREACHED();
}

- (void)endDrag {
  [self highlight:NO];
}

@end  // @interface DraggableButton
