// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>

@class BookmarkItem;
@class BookmarkManagerController;


// Controller for the bookmark tree view (the right pane).
@interface BookmarkTreeController : NSObject <NSUserInterfaceValidations> {
 @private
  IBOutlet NSOutlineView* outline_;
  IBOutlet BookmarkManagerController* manager_;
  BookmarkItem* group_;
  BOOL showsLeaves_;
  BOOL flat_;
}

// If set, leaf (URL) bookmarks are shown; if not, they're hidden.
@property BOOL showsLeaves;
// If set, displays a flat list (no hierarchy).
@property BOOL flat;
// Controls the visibility of the "Folder" column (used with search/recents.)
@property BOOL showsFolderColumn;

// The top-level bookmark folder used as the root of the outline's tree.
// Observable, bindable.
@property (assign) BookmarkItem* group;
// Controls whether leaf bookmarks or just folders are shown.
// The currently selected item(s) in the outline. (Observable.)
@property (retain) NSArray* selectedItems;
// The currently selected single item; nil on multiple selection. (Observable.)
@property (retain) BookmarkItem* selectedItem;
// The currently selected or right-clicked items, for commands to act on.
@property (readonly) NSArray* actionItems;

// The parent folder and index at which items should be inserted/pasted.
// Returns NO if insertion is not allowed.
- (BOOL)getInsertionParent:(BookmarkItem**)outParent
                     index:(NSUInteger*)outIndex;
// Just returns whether insertion is allowed.
- (BOOL)canInsert;

// Expands a folder item (including any parent folders).
// Returns YES on success, NO if it couldn't find the folder.
- (BOOL)expandItem:(BookmarkItem*)item;
// Expands parent folders, selects the item, and scrolls it into view:
// Returns YES on success, NO if it couldn't find the item.
- (BOOL)revealItem:(BookmarkItem*)item;

// Action for the New Folder command.
- (IBAction)newFolder:(id)sender;

// Action for the Delete command; also invoked by the delete key.
- (IBAction)delete:(id)sender;

// Reveals the selected search/recent item in its real folder.
- (IBAction)revealSelectedItem:(id)sender;

// Opens the selected bookmark(s) in new tabs.
- (IBAction)openItems:(id)sender;

// Makes the selected bookmark's title editable.
- (IBAction)editTitle:(id)sender;

// Returns YES if an action should be enabled.
- (BOOL)validateAction:(SEL)action;

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
// The owning BookmarkTreeController (same as the delegate).
@property (readonly) BookmarkTreeController* bookmarkController;
@end
