// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/ui/cocoa/tab_contents/web_contents_drag_source.h"

// A class that handles tracking and event processing for a drag and drop
// originating from the content area.
@interface BookmarkDragSource : WebContentsDragSource {
 @private
  // Our drop data. Should only be initialized once.
  std::vector<BookmarkNodeData::Element> dropData_;

  Profile* profile_;
}

// Initialize a DragDataSource object for a drag (originating on the given
// contentsView and with the given dropData and pboard). Fill the pasteboard
// with data types appropriate for dropData.
- (id)initWithContentsView:(TabContentsViewCocoa*)contentsView
                  dropData:
                      (const std::vector<BookmarkNodeData::Element>&)dropData
                   profile:(Profile*)profile
                pasteboard:(NSPasteboard*)pboard
         dragOperationMask:(NSDragOperation)dragOperationMask;

@end
