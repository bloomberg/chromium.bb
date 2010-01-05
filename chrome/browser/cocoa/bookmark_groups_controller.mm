// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_groups_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#import "third_party/apple/ImageAndTextCell.h"


@interface BookmarkGroupsController ()
- (void)syncSelection;
- (void)reload;
@end


@implementation BookmarkGroupsController

@synthesize groups = groups_, selectedGroup = selectedGroup_;

-(void)dealloc {
  [groups_ release];
  [selectedGroup_ release];
  [super dealloc];
}

// Completely reloads the contents of the table view.
- (void)reload {
  NSMutableArray* groups = [NSMutableArray array];
  BookmarkModel* bookmarkModel = [manager_ bookmarkModel];
  const BookmarkNode* node = bookmarkModel->GetBookmarkBarNode();
  [groups addObject:[manager_ itemFromNode:node]];
  node = bookmarkModel->other_node();
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_folder()) {
      [groups addObject:[manager_ itemFromNode:child]];
    }
  }
  //TODO(snej): Append Recents and Search groups

  if (![groups isEqual:[self groups]]) {
    [self setGroups:groups];
    [groupsTable_ reloadData];
    [self syncSelection];
  }
}

// Responds to changes in the bookmark data model.
- (void)nodeChanged:(const BookmarkNode*)node
    childrenChanged:(BOOL)childrenChanged {
  if (node == [manager_ bookmarkModel]->other_node()) {
    [self reload];
  }
}

// Returns the selected BookmarkNode.
- (const BookmarkNode*)selectedNode {
  NSInteger row = [groupsTable_ selectedRow];
  if (row < 0)
    return NULL;
  id item = [groups_ objectAtIndex:row];
  return [manager_ nodeFromItem:item];
}

- (id)selectedGroup {
  return selectedGroup_;
}

- (void)setSelectedGroup:(id)group {
  [selectedGroup_ autorelease];
  selectedGroup_ = [group retain];

  NSInteger row = group ? [groups_ indexOfObject:group] : NSNotFound;
  if (row != NSNotFound)
    [groupsTable_ selectRow:row byExtendingSelection:NO];
  else
    [groupsTable_ deselectAll:self];
}

- (NSTableView*)groupsTable {
  return groupsTable_;
}


#pragma mark -
#pragma mark COMMANDS:


// Updates the |selectedGroup| property based on the table view's selection.
- (void)syncSelection {
  id selGroup = nil;
  NSInteger row = [groupsTable_ selectedRow];
  if (row >= 0) {
    selGroup = [groups_ objectAtIndex:row];
    if (![selGroup isKindOfClass:[NSValue class]])
      selGroup = nil;
  }
  if (selGroup != [self selectedGroup])
      [self setSelectedGroup:selGroup];
}

// Called when a row is clicked; updates the |selectedGroup| property.
- (IBAction)tableClicked:(id)sender {
  [self syncSelection];
}

// The Delete command; also invoked by the delete key.
- (IBAction)delete:(id)sender {
  const BookmarkNode* sel = [self selectedNode];
  if (!sel) {
    NSBeep();
    return;
  }
  const BookmarkNode* parent = sel->GetParent();
  [manager_ bookmarkModel]->Remove(parent, parent->IndexOfChild(sel));
}


#pragma mark -
#pragma mark LIST VIEW:


// Returns the number of rows in the table (NSTableView data source).
- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  return [groups_ count];
}

// Returns the contents of a table cell (NSTableView data source).
- (id)              tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)row {
  id item = [groups_ objectAtIndex:row];
  return base::SysWideToNSString([manager_ nodeFromItem:item]->GetTitle());
}

// Sets a table cell's icon before it's drawn (NSTableView delegate).
- (void)tableView:(NSTableView*)tableView
  willDisplayCell:(id)cell
   forTableColumn:(NSTableColumn*)tableColumn
              row:(NSInteger)row {
  id item = [groups_ objectAtIndex:row];
  [(ImageAndTextCell*)cell setImage:[manager_ iconForItem:item]];
}

@end


@implementation BookmarksTableView

- (IBAction)delete:(id)sender {
  [(BookmarkGroupsController*)[self delegate] delete:sender];
}

- (void)keyDown:(NSEvent*)event {
  if ([event keyCode] == 51)      // Delete key
    [self delete:self];
  else
    [super keyDown:event];
}

@end
