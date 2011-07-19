// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_WEB_CONTENTS_DRAG_SOURCE_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_WEB_CONTENTS_DRAG_SOURCE_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"

@class TabContentsViewCocoa;

// A class that handles tracking and event processing for a drag and drop
// originating from the content area.  Subclasses should implement
// fillClipboard and dragImage.
@interface WebContentsDragSource : NSObject {
 @private
  // Our tab. Weak reference (owns or co-owns us).
  TabContentsViewCocoa* contentsView_;

  // Our pasteboard.
  scoped_nsobject<NSPasteboard> pasteboard_;

  // A mask of the allowed drag operations.
  NSDragOperation dragOperationMask_;
}

// Initialize a DragDataSource object for a drag (originating on the given
// contentsView and with the given dropData and pboard). Fill the pasteboard
// with data types appropriate for dropData.
- (id)initWithContentsView:(TabContentsViewCocoa*)contentsView
                pasteboard:(NSPasteboard*)pboard
         dragOperationMask:(NSDragOperation)dragOperationMask;

// Creates the drag image.  Implemented by the subclass.
- (NSImage*)dragImage;

// Put the data being dragged onto the pasteboard.  Implemented by the
// subclass.
- (void)fillPasteboard;

// Returns a mask of the allowed drag operations.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal;

// Start the drag (on the originally provided contentsView); can do this right
// after -initWithContentsView:....
- (void)startDrag;

// End the drag and clear the pasteboard; hook up to
// -draggedImage:endedAt:operation:.
- (void)endDragAt:(NSPoint)screenPoint
        operation:(NSDragOperation)operation;

// Drag moved; hook up to -draggedImage:movedTo:.
- (void)moveDragTo:(NSPoint)screenPoint;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_WEB_CONTENTS_DRAG_SOURCE_H_
