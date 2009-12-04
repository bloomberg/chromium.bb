// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/url_drop_target.h"

#include "base/logging.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

@interface URLDropTargetHandler(Private)

// Get the window controller.
- (id<URLDropTargetWindowController>)windowController;

// Gets the appropriate drag operation given the |NSDraggingInfo|.
- (NSDragOperation)getDragOperation:(id<NSDraggingInfo>)sender;

// Tell the window controller to hide the drop indicator.
- (void)hideIndicator;

@end  // @interface URLDropTargetHandler(Private)

@implementation URLDropTargetHandler

- (id)initWithView:(NSView*)view {
  if ((self = [super init])) {
    view_ = view;
    [view_ registerForDraggedTypes:
         [NSArray arrayWithObjects:kWebURLsWithTitlesPboardType,
                                   NSURLPboardType,
                                   NSStringPboardType,
                                   NSFilenamesPboardType,
                                   nil]];
  }
  return self;
}

// The following four methods implement parts of the |NSDraggingDestination|
// protocol, which the owner should "forward" to its |URLDropTargetHandler|
// (us).

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return [self getDragOperation:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  NSDragOperation dragOp = [self getDragOperation:sender];
  if (dragOp == NSDragOperationCopy) {
    // Just tell the window controller to update the indicator.
    [[self windowController] indicateDropURLsAt:[sender draggingLocation]];
  }
  return dragOp;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  [self hideIndicator];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  [self hideIndicator];

  NSPasteboard* pboard = [sender draggingPasteboard];
  if ([pboard containsURLData]) {
    NSArray* urls = nil;
    NSArray* titles;  // discarded
    [pboard getURLs:&urls andTitles:&titles];

    if ([urls count]) {
      // Tell the window controller about the dropped URL(s).
      [[self windowController] dropURLs:urls at:[sender draggingLocation]];
      return YES;
    }
  }

  return NO;
}

@end  // @implementation URLDropTargetHandler

@implementation URLDropTargetHandler(Private)

- (id<URLDropTargetWindowController>)windowController {
  id<URLDropTargetWindowController> controller =
      [[view_ window] windowController];
  DCHECK([(id)controller conformsToProtocol:
      @protocol(URLDropTargetWindowController)]);
  return controller;
}

- (NSDragOperation)getDragOperation:(id<NSDraggingInfo>)sender {
  // Only allow the copy operation.
  return [sender draggingSourceOperationMask] & NSDragOperationCopy;
}

- (void)hideIndicator {
  [[self windowController] hideDropURLsIndicator];
}

@end  // @implementation URLDropTargetHandler(Private)
