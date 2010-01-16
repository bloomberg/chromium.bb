// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

@class BookmarkItem;
@class BookmarkTreeController;
class BookmarkManagerBridge;
class BookmarkModel;
class BookmarkNode;
@class FakeBookmarkItem;
class Profile;

// Controller for the bookmark manager window. There is at most one instance.
@interface BookmarkManagerController : NSWindowController {
 @private
  IBOutlet NSSearchField* toolbarSearchView_;
  IBOutlet BookmarkTreeController* groupsController_;
  IBOutlet BookmarkTreeController* listController_;

  Profile* profile_;  // weak
  scoped_ptr<BookmarkManagerBridge> bridge_;
  scoped_nsobject<NSMapTable> nodeMap_;
  scoped_nsobject<FakeBookmarkItem> root_;  // Root of tree
  scoped_nsobject<FakeBookmarkItem> searchGroup_;  // Search Results group item
  scoped_nsobject<FakeBookmarkItem> recentGroup_;  // Recently-Added group item
}

// Opens the bookmark manager window, or brings it to the front if it's open.
+ (BookmarkManagerController*)showBookmarkManager:(Profile*)profile;

// The user Profile.
@property (readonly) Profile* profile;

// The BookmarkModel of the manager's Profile.
@property (readonly) BookmarkModel* bookmarkModel;

@property (readonly) BookmarkItem* bookmarkBarItem;
@property (readonly) BookmarkItem* otherBookmarksItem;

// Maps C++ BookmarkNode objects to Objective-C BookmarkItems.
- (BookmarkItem*)itemFromNode:(const BookmarkNode*)node;

// Shows a group and its contents.
- (void)showGroup:(BookmarkItem*)group;
// Shows and selects a particular bookmark in its group.
- (BOOL)revealItem:(BookmarkItem*)item;

- (void)setSearchString:(NSString*)string;

// Called by the toolbar search field after the user changes its text.
- (IBAction)searchFieldChanged:(id)sender;
// Called by the toolbar item; forwards to the focused tree controller.
- (IBAction)delete:(id)sender;
// Called by the toolbar item; forwards to the focused tree controller.
- (IBAction)newFolder:(id)sender;

@end


// Exposed only for unit tests.
@interface BookmarkManagerController (UnitTesting)

- (void)forgetNode:(const BookmarkNode*)node;
@property (readonly) BookmarkTreeController* groupsController;
@property (readonly) BookmarkTreeController* listController;
@property (readonly) FakeBookmarkItem* searchGroup;
@property (readonly) FakeBookmarkItem* recentGroup;

@end
