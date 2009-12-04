// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_strip_view.h"

#include "base/logging.h"

@implementation TabStripView

@synthesize newTabButton = newTabButton_;

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Set lastMouseUp_ = -1000.0 so that timestamp-lastMouseUp_ is big unless
    // lastMouseUp_ has been reset.
    lastMouseUp_ = -1000.0;

    // Register to be an URL drop target.
    dropHandler_.reset([[URLDropTargetHandler alloc] initWithView:self]);
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  NSRect boundsRect = [self bounds];
  NSRect borderRect, contentRect;
  NSDivideRect(boundsRect, &borderRect, &contentRect, 1, NSMinYEdge);
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.2] set];

  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

// We accept first mouse so clicks onto close/zoom/miniaturize buttons and
// title bar double-clicks are properly detected even when the window is in the
// background.
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  return YES;
}

// Trap double-clicks and make them miniaturize the browser window.
- (void)mouseUp:(NSEvent*)event {
  NSInteger clickCount = [event clickCount];
  NSTimeInterval timestamp = [event timestamp];

  // Double-clicks on Zoom/Close/Mininiaturize buttons shouldn't cause
  // miniaturization. For those, we miss the first click but get the second
  // (with clickCount == 2!). We thus check that we got a first click shortly
  // before (measured up-to-up) a double-click. Cocoa doesn't have a documented
  // way of getting the proper interval (= (double-click-threshold) +
  // (drag-threshold); the former is Carbon GetDblTime()/60.0 or
  // com.apple.mouse.doubleClickThreshold [undocumented]). So we hard-code
  // "short" as 0.8 seconds. (Measuring up-to-up isn't enough to properly
  // detect double-clicks, but we're actually using Cocoa for that.)
  if (clickCount == 2 && (timestamp - lastMouseUp_) < 0.8) {
    // We use an undocumented method in Cocoa; if it doesn't exist, default to
    // YES. If it ever goes away, we can do (using an undocumented pref. key):
    //   NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    //   if (![defaults objectForKey:@"AppleMiniaturizeOnDoubleClick"]
    //       || [defaults boolForKey:@"AppleMiniaturizeOnDoubleClick"])
    //     [[self window] performMiniaturize:self];
    DCHECK([NSWindow
        respondsToSelector:@selector(_shouldMiniaturizeOnDoubleClick)]);
    if (![NSWindow
            respondsToSelector:@selector(_shouldMiniaturizeOnDoubleClick)]
        || [NSWindow
            performSelector:@selector(_shouldMiniaturizeOnDoubleClick)])
      [[self window] performMiniaturize:self];
  } else {
    [super mouseUp:event];
  }

  // If clickCount is 0, the drag threshold was passed.
  lastMouseUp_ = (clickCount == 1) ? timestamp : -1000.0;
}

// Required by |URLDropTargetHandler|.
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return [dropHandler_ draggingEntered:sender];
}

// Required by |URLDropTargetHandler|.
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  return [dropHandler_ draggingUpdated:sender];
}

// Required by |URLDropTargetHandler|.
- (void)draggingExited:(id<NSDraggingInfo>)sender {
  return [dropHandler_ draggingExited:sender];
}

// Required by |URLDropTargetHandler|.
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  return [dropHandler_ performDragOperation:sender];
}

@end
