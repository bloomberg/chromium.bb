// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_URL_DROP_TARGET_H_
#define CHROME_BROWSER_COCOA_URL_DROP_TARGET_H_

#import <Cocoa/Cocoa.h>

@interface URLDropTargetHandler : NSObject {
 @private
  NSView* view_;  // weak
}

// Initialize the given view to accept drops of URLs; this requires the view's
// window's controller to implement the |URLDropTargetWindowController| protocol
// (below).
- (id)initWithView:(NSView*)view;

// The owner view should implement the following methods by calling the
// |URLDropTargetHandler|'s version, and leave the others to the default
// implementation provided by |NSView|/|NSWindow|.
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender;
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender;
- (void)draggingExited:(id<NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender;

@end  // @interface URLDropTargetHandler

@protocol URLDropTargetWindowController

// The given URLs (an |NSArray| of |NSString|s) were dropped at the given
// location (location in window base coordinates).
- (void)dropURLs:(NSArray*)urls at:(NSPoint)location;

// Dragging is in progress over the owner view (at the given location, in window
// base coordinates) and any indicator of location -- e.g., an arrow -- should
// be updated/shown.
- (void)indicateDropURLsAt:(NSPoint)location;

// Dragging is over, and any indicator should be hidden.
- (void)hideDropURLsIndicator;

@end  // @protocol URLDropTargetWindowController

#endif  // CHROME_BROWSER_COCOA_URL_DROP_TARGET_H_
