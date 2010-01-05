// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@class BookmarkManagerController;
class BookmarkModel;
class BookmarkNode;


// Controller for the bookmark group list (the left pane).
@interface BookmarkGroupsController : NSObject {
 @private
  IBOutlet NSTableView* groupsTable_;
  IBOutlet BookmarkManagerController* manager_;

  NSMutableArray* groups_;  // array of node items ('id's)
  id selectedGroup_;        // selected item from groups
}

// The ordered list of groups shown in the table view. Observable.
// Each item is an 'id', as vended by BookmarkManagerController's -itemFromNode:
@property (copy) NSArray* groups;
// The item in -groups currently selected in the table view. Observable.
@property (retain) id selectedGroup;

// Called by the table view when a row is clicked.
- (IBAction)tableClicked:(id)sender;

// Reloads the groups property and table contents from the data model.
- (void)reload;

// Called by BookmarkManagerController to notify that the data model's changed.
- (void)nodeChanged:(const BookmarkNode*)node
    childrenChanged:(BOOL)childrenChanged;

@end


// Exposed only for unit tests.
@interface BookmarkGroupsController (UnitTesting)

@property (readonly) NSTableView* groupsTable;

@end



// Table view for bookmark group list; handles Cut/Copy/Paste and Delete key.
@interface BookmarksTableView : NSTableView
@end
