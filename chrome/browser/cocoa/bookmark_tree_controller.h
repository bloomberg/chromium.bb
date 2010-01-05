// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>

@class BookmarkManagerController;
class BookmarkModel;
class BookmarkNode;


// Controller for the bookmark tree view (the right pane).
@interface BookmarkTreeController : NSObject {
 @private
  IBOutlet NSOutlineView* outline_;
  IBOutlet BookmarkManagerController* manager_;
  id group_;
  std::vector<const BookmarkNode*> draggedNodes_;
}

// The top-level bookmark folder used as the root of the outline's tree.
// Observable, bindable.
@property (assign) id group;
// The currently selected item(s) in the outline. (Not observable.)
@property (retain) NSArray* selectedItems;

// Action for the Delete command; also invoked by the delete key.
- (IBAction)delete:(id)sender;

// Maps BookmarkNodes to NSOutlineView items. Equivalent to the method on
// BookmarkManagerController except that it maps the root node to nil.
- (id)itemFromNode:(const BookmarkNode*)node;

// Maps NSOutlineView items back to BookmarkNodes. Equivalent to the method on
// BookmarkManagerController except that it maps nil back to the root node.
- (const BookmarkNode*)nodeFromItem:(id)item;

// Called by the BookmarkManagerController to notify the data model's changed.
- (void)itemChanged:(id)nodeItem childrenChanged:(BOOL)childrenChanged;

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
- (BOOL)copyToPasteboard:(NSPasteboard*)pb;
- (BOOL)pasteFromPasteboard:(NSPasteboard*)pb;
- (const BookmarkNode*)nodeForDropOnItem:(id)item
                           proposedIndex:(NSInteger*)childIndex;
- (void)moveNodes:(std::vector<const BookmarkNode*>)nodes
         toFolder:(const BookmarkNode*)dstParent
          atIndex:(int)dstIndex;
@end


// Outline view for bookmark tree; handles Cut/Copy/Paste and Delete key.
@interface BookmarksOutlineView : NSOutlineView
@end
