// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>

@class BookmarkItem;
@class BookmarkManagerController;


// Controller for the bookmark tree view (the right pane).
@interface BookmarkTreeController : NSObject {
 @private
  IBOutlet NSOutlineView* outline_;
  IBOutlet BookmarkManagerController* manager_;
  BookmarkItem* group_;
}

// The top-level bookmark folder used as the root of the outline's tree.
// Observable, bindable.
@property (assign) BookmarkItem* group;
// Controls whether leaf bookmarks or just folders are shown.
// The currently selected item(s) in the outline. (Not observable.)
@property (retain) NSArray* selectedItems;

// Action for the Delete command; also invoked by the delete key.
- (IBAction)delete:(id)sender;

// Called by the BookmarkManagerController to notify the data model's changed.
- (void)itemChanged:(BookmarkItem*)nodeItem
    childrenChanged:(BOOL)childrenChanged;

@end


// Drag/drop and copy/paste methods
// (These are implemented in bookmark_tree_controller_paste.mm.)
@interface BookmarkTreeController (Pasteboard)
// One-time drag-n-drop setup; called from -awakeFromNib.
- (void)registerDragTypes;
- (IBAction)cut:(id)sender;
- (IBAction)copy:(id)sender;
- (IBAction)paste:(id)sender;
@end


// Exposed only for unit tests.
@interface BookmarkTreeController (UnitTesting)
@property (readonly) NSOutlineView* outline;
- (NSArray*)readPropertyListFromPasteboard:(NSPasteboard*)pb;
- (BOOL)copyToPasteboard:(NSPasteboard*)pb;
- (BOOL)pasteFromPasteboard:(NSPasteboard*)pb;
- (BookmarkItem*)itemForDropOnItem:(BookmarkItem*)item
                     proposedIndex:(NSInteger*)childIndex;
- (void)moveItems:(NSArray*)items
         toFolder:(BookmarkItem*)dstParent
          atIndex:(int)dstIndex;
@end


// Outline view for bookmark tree; handles Cut/Copy/Paste and Delete key.
@interface BookmarksOutlineView : NSOutlineView
@end
