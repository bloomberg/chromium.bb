// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_groups_controller.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#import "third_party/apple/ImageAndTextCell.h"


@interface BookmarkGroupsController ()
- (void)syncSelection;
- (void)reload;

// Returns YES if the text in the identified cell is editable.
- (BOOL)        tableView:(NSTableView*)tableView
    shouldEditTableColumn:(NSTableColumn*)tableColumn
                      row:(NSInteger)row;
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
  [groups addObject:[manager_ bookmarkBarItem]];
  BookmarkItem* other = [manager_ otherBookmarksItem];
  for (NSUInteger i = 0; i < [other numberOfChildren]; i++) {
    BookmarkItem* child = [other childAtIndex:i];
    if ([child isFolder]) {
      [groups addObject:child];
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
- (void)itemChanged:(BookmarkItem*)item
    childrenChanged:(BOOL)childrenChanged {
  if (item == [manager_ otherBookmarksItem]) {
    [self reload];
  }
}

// Returns the selected BookmarkNode.
- (BookmarkItem*)selectedNode {
  NSInteger row = [groupsTable_ selectedRow];
  if (row < 0)
    return NULL;
  return [groups_ objectAtIndex:row];
}

- (BookmarkItem*)selectedGroup {
  return selectedGroup_;
}

- (void)setSelectedGroup:(BookmarkItem*)group {
  [selectedGroup_ autorelease];
  selectedGroup_ = [group retain];

  NSInteger row = group ? [groups_ indexOfObject:group] : NSNotFound;
  if (row != NSNotFound) {
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:row];
    [groupsTable_ selectRowIndexes:indexSet byExtendingSelection:NO];
  } else {
    [groupsTable_ deselectAll:self];
  }
}

// Updates the |selectedGroup| property based on the table view's selection.
- (void)syncSelection {
  BookmarkItem* selGroup = nil;
  NSInteger row = [groupsTable_ selectedRow];
  if (row >= 0) {
    selGroup = [groups_ objectAtIndex:row];
  }
  if (selGroup != [self selectedGroup])
      [self setSelectedGroup:selGroup];
}

- (NSTableView*)groupsTable {
  return groupsTable_;
}

// Returns the selected/right-clicked row #, or -1 if none.
- (int)actionRow {
  int row = [groupsTable_ clickedRow];
  if (row < 0)
    row = [groupsTable_ selectedRow];
  return row;
}

// Returns the selected/right-clicked item, or nil if none.
- (BookmarkItem*)actionItem {
  int row = [self actionRow];
  return row >= 0 ? [groups_ objectAtIndex:row] : nil;
}


#pragma mark -
#pragma mark COMMANDS:


// Called when a row is clicked; updates the |selectedGroup| property.
- (IBAction)tableClicked:(id)sender {
  [self syncSelection];
}

// The Delete command; also invoked by the delete key.
- (IBAction)delete:(id)sender {
  BookmarkItem* item = [self actionItem];
  if (!item) {
    NSBeep();
    return;
  }
  [[item parent] removeChild: item];
}

// Makes the title of the selected row editable.
- (IBAction)editTitle:(id)sender {
  int row = [self actionRow];
  if (row < 0) {
    NSBeep();
    return;
  }
  if (![self tableView:groupsTable_
      shouldEditTableColumn:[[groupsTable_ tableColumns] objectAtIndex:0]
                   row:row]) {
    NSBeep();
    return;
  }
  [groupsTable_ editColumn:0
                       row:row
                 withEvent:[NSApp currentEvent]
                    select:YES];
}

// Creates a new folder.
- (IBAction)newFolder:(id)sender {
  BookmarkItem* targetNode;
  NSInteger childIndex;
  BookmarkItem* item = [self actionItem];
  if (item) {
    // Insert at selected/clicked row.
    targetNode = [item parent];
    childIndex = [targetNode indexOfChild: item];
  } else {
    // ...or at very end if there's no selection:
    targetNode = [manager_ otherBookmarksItem];
    childIndex = [targetNode numberOfChildren];
  }

  BookmarkItem* folder = [targetNode addFolderWithTitle: @"" atIndex: childIndex];

  // Edit the title:
  [self setSelectedGroup:folder];
  int row = [groups_ indexOfObject:folder];
  DCHECK(row!=NSNotFound);
  [groupsTable_ editColumn:0
                       row:row
                 withEvent:[NSApp currentEvent]
                    select:YES];
}

// Selectively enables/disables menu commands.
- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  SEL action = [menuItem action];
  if (action == @selector(delete:) || action == @selector(editTitle:)) {
    // Note ">", not ">=". Row 0 is Bookmarks Bar, which is immutable.
    return [self actionRow] > 0;
  } else {
    return YES;
  }
}

- (NSMenu*)menu {
  NSMenu* menu = [manager_ contextMenu];
  for (NSMenuItem* item in [menu itemArray])
    [item setTarget:self];
  return menu;
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
  BookmarkItem* item = [groups_ objectAtIndex:row];
  return [item title];
}

- (void)tableView:(NSTableView*)tableView
   setObjectValue:(id)value
   forTableColumn:(NSTableColumn*)tableColumn
              row:(NSInteger)row {

  BookmarkItem* item = [groups_ objectAtIndex:row];
  [item setTitle:value];
}

- (BOOL)        tableView:(NSTableView*)tableView
    shouldEditTableColumn:(NSTableColumn*)tableColumn
                      row:(NSInteger)row {
  BookmarkItem* item = [groups_ objectAtIndex:row];
  return ![item isFixed];
}

// Sets a table cell's icon before it's drawn (NSTableView delegate).
- (void)tableView:(NSTableView*)tableView
  willDisplayCell:(id)cell
   forTableColumn:(NSTableColumn*)tableColumn
              row:(NSInteger)row {
  BookmarkItem* item = [groups_ objectAtIndex:row];
  [(ImageAndTextCell*)cell setImage:[item icon]];
}

@end


@implementation BookmarksTableView

- (IBAction)delete:(id)sender {
  [(BookmarkGroupsController*)[self delegate] delete:sender];
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  return [(BookmarkGroupsController*)[self delegate] validateMenuItem:menuItem];
}

- (void)keyDown:(NSEvent*)event {
  NSString* chars = [event charactersIgnoringModifiers];
  if ([chars length] == 1) {
    switch ([chars characterAtIndex:0]) {
      case NSDeleteCharacter:
      case NSDeleteFunctionKey:
        [self delete:self];
        return;
      case NSCarriageReturnCharacter:
      case NSEnterCharacter:
        [(BookmarkGroupsController*)[self delegate] editTitle:self];
        return;
      case NSTabCharacter:
        // For some reason NSTableView responds to the tab key by editing
        // the selected row. Override this with the normal behavior.
        [[self window] selectNextKeyView:self];
        return;
    }
  }
  [super keyDown:event];
}

- (NSMenu*)menu {
  return [(BookmarkGroupsController*)[self delegate] menu];
}

@end
