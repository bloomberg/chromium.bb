// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"

@class BookmarkGroupsController;
@class BookmarkTreeController;
class BookmarkManagerBridge;
class BookmarkModel;
class BookmarkNode;
class Profile;

// Controller for the bookmark manager window. There is at most one instance.
@interface BookmarkManagerController : NSWindowController {
 @private
  IBOutlet NSTableView* groupsTable_;
  IBOutlet NSSearchField* toolbarSearchView_;
  IBOutlet BookmarkGroupsController* groupsController_;
  IBOutlet BookmarkTreeController* treeController_;

  Profile* profile_;  // weak
  BookmarkManagerBridge* bridge_;
  scoped_nsobject<NSMapTable> nodeMap_;
  scoped_nsobject<NSImage> folderIcon_;
  scoped_nsobject<NSImage> defaultFavIcon_;
}

// Opens the bookmark manager window, or brings it to the front if it's open.
+ (BookmarkManagerController*)showBookmarkManager:(Profile*)profile;

// The BookmarkModel of the manager's Profile.
@property (readonly) BookmarkModel* bookmarkModel;

// Maps C++ BookmarkNode objects to opaque Objective-C objects.
// This allows nodes to be stored in NSArrays or NSOutlineViews.
- (id)itemFromNode:(const BookmarkNode*)node;

// Converse of -nodeFromItem: -- maps an opaque item back to a BookmarkNode.
- (const BookmarkNode*)nodeFromItem:(id)item;

// Returns the icon to be displayed for an item representing a BookmarkNode.
// This will be the URL's favicon, a generic page icon, or a folder icon.
- (NSImage*)iconForItem:(id)item;

// Opens a URL bookmark in a browser tab.
- (void)openBookmarkItem:(id)item;

// Called by the toolbar search field after the user changes its text.
- (IBAction)searchFieldChanged:(id)sender;

@end


// Exposed only for unit tests.
@interface BookmarkManagerController (UnitTesting)

- (void)forgetNode:(const BookmarkNode*)node;
@property (readonly) BookmarkGroupsController* groupsController;
@property (readonly) BookmarkTreeController* treeController;

@end
