// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/url_drop_target.h"

#include "base/basictypes.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"

@interface URLDropTargetHandler(Private)

// Gets the appropriate drag operation given the |NSDraggingInfo|.
- (NSDragOperation)getDragOperation:(id<NSDraggingInfo>)sender;

// Tell the window controller to hide the drop indicator.
- (void)hideIndicator;

@end  // @interface URLDropTargetHandler(Private)

@implementation URLDropTargetHandler

+ (NSArray*)handledDragTypes {
  return [NSArray arrayWithObjects:kWebURLsWithTitlesPboardType,
                                   NSURLPboardType,
                                   NSStringPboardType,
                                   NSFilenamesPboardType,
                                   nil];
}

- (id)initWithView:(NSView<URLDropTarget>*)view {
  if ((self = [super init])) {
    view_ = view;
    [view_ registerForDraggedTypes:[URLDropTargetHandler handledDragTypes]];
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
    NSPoint hoverPoint = [view_ convertPoint:[sender draggingLocation]
                                    fromView:nil];
    [[view_ urlDropController] indicateDropURLsInView:view_ at:hoverPoint];
  }
  return dragOp;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  [self hideIndicator];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  [self hideIndicator];

  NSPasteboard* pboard = [sender draggingPasteboard];
  NSArray* supportedTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
  NSString* bestType = [pboard availableTypeFromArray:supportedTypes];

  NSPoint dropPoint =
      [view_ convertPoint:[sender draggingLocation] fromView:nil];
  // Tell the window controller about the dropped URL(s).
  if ([pboard containsURLData]) {
    NSArray* urls = nil;
    NSArray* titles;  // discarded
    [pboard getURLs:&urls andTitles:&titles convertingFilenames:YES];

    if ([urls count]) {
      [[view_ urlDropController] dropURLs:urls inView:view_ at:dropPoint];
      return YES;
    }
  } else if (NSString* text = [pboard stringForType:bestType]) {
    // This does not include any URLs, so treat it as plain text if we can
    // get NSString.
    [[view_ urlDropController] dropText:text inView:view_ at:dropPoint];
    return YES;
  }

  return NO;
}

@end  // @implementation URLDropTargetHandler

@implementation URLDropTargetHandler(Private)

- (NSDragOperation)getDragOperation:(id<NSDraggingInfo>)sender {
  NSPasteboard* pboard = [sender draggingPasteboard];
  NSArray *supportedTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
  NSString *bestType = [pboard availableTypeFromArray:supportedTypes];
  if (![pboard containsURLData] && ![pboard stringForType:bestType])
    return NSDragOperationNone;

  // Only allow the copy operation.
  return [sender draggingSourceOperationMask] & NSDragOperationCopy;
}

- (void)hideIndicator {
  [[view_ urlDropController] hideDropURLsIndicatorInView:view_];
}

@end  // @implementation URLDropTargetHandler(Private)
