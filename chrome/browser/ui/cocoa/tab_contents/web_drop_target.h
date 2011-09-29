// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/string16.h"

class GURL;
class RenderViewHost;
class TabContents;
class TabContentsWrapper;
struct WebDropData;

// A typedef for a RenderViewHost used for comparison purposes only.
typedef RenderViewHost* RenderViewHostIdentifier;

// A class that handles tracking and event processing for a drag and drop
// over the content area. Assumes something else initiates the drag, this is
// only for processing during a drag.

@interface WebDropTarget : NSObject {
 @private
  // Our associated TabContents. Weak reference.
  TabContents* tabContents_;

  // The TabContentsWrapper for |tab_contents_|.
  // Weak reference; may be NULL if the contents aren't contained in a wrapper
  // (e.g. WebUI dialogs).
  TabContentsWrapper* tab_;

  // Updated asynchronously during a drag to tell us whether or not we should
  // allow the drop.
  NSDragOperation current_operation_;

  // Keep track of the render view host we're dragging over.  If it changes
  // during a drag, we need to re-send the DragEnter message.
  RenderViewHostIdentifier currentRVH_;
}

// |contents| is the TabContents representing this tab, used to communicate
// drag&drop messages to WebCore and handle navigation on a successful drop
// (if necessary).
- (id)initWithTabContents:(TabContents*)contents;

// Sets the current operation negotiated by the source and destination,
// which determines whether or not we should allow the drop. Takes effect the
// next time |-draggingUpdated:| is called.
- (void)setCurrentOperation: (NSDragOperation)operation;

// Messages to send during the tracking of a drag, ususally upon receiving
// calls from the view system. Communicates the drag messages to WebCore.
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info
                              view:(NSView*)view;
- (void)draggingExited:(id<NSDraggingInfo>)info;
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info
                              view:(NSView*)view;
- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
                              view:(NSView*)view;

@end

// Public use only for unit tests.
@interface WebDropTarget(Testing)
// Given |data|, which should not be nil, fill it in using the contents of the
// given pasteboard.
- (void)populateWebDropData:(WebDropData*)data
             fromPasteboard:(NSPasteboard*)pboard;
// Given a point in window coordinates and a view in that window, return a
// flipped point in the coordinate system of |view|.
- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint
                            view:(NSView*)view;
// Given a point in window coordinates and a view in that window, return a
// flipped point in screen coordinates.
- (NSPoint)flipWindowPointToScreen:(const NSPoint&)windowPoint
                              view:(NSView*)view;
@end
