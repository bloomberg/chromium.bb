// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_tree_controller.h"

#include "app/l10n_util_mac.h"
#include "base/logging.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#include "grit/generated_resources.h"

// Outline-view column identifiers.
static NSString* const kTitleColIdent = @"title";
static NSString* const kURLColIdent = @"url";
static NSString* const kFolderColIdent = @"folder";


@implementation BookmarkTreeController

// Allow the |group| property to be bound (by BookmarkManagerController.)
+ (void)initialize {
  [self exposeBinding:@"group"];
}

// Initialization after the nib is loaded.
- (void)awakeFromNib {
  [outline_ setTarget:self];
  [outline_ setDoubleAction:@selector(openItems:)];
  [self registerDragTypes];
}

- (BookmarkItem*)group {
  return group_;
}

- (void)setGroup:(BookmarkItem*)group {
  if (group != group_) {
    group_ = group;

    [outline_ deselectAll:self];
    [outline_ reloadData];
    [outline_ noteNumberOfRowsChanged];
  }
}

- (NSOutlineView*)outline {
  return outline_;
}

// Updates the tree after the data model has changed.
- (void)itemChanged:(BookmarkItem*)nodeItem childrenChanged:(BOOL)childrenChanged {
  if (nodeItem == group_)
    nodeItem = nil;
  [outline_ reloadItem:nodeItem reloadChildren:childrenChanged];
}

// Getter for the |selectedItems| property.
- (NSArray*)selectedItems {
  NSMutableArray* items = [NSMutableArray array];
  NSIndexSet* selectedRows = [outline_ selectedRowIndexes];
  if (selectedRows != nil) {
    for (NSInteger row = [selectedRows firstIndex]; row != NSNotFound;
         row = [selectedRows indexGreaterThanIndex:row]) {
      [items addObject:[outline_ itemAtRow:row]];
    }
  }
  return items;
}

// Setter for the |selectedItems| property.
- (void)setSelectedItems:(NSArray*)items {
  NSMutableIndexSet* newSelection = [NSMutableIndexSet indexSet];

  for (NSUInteger i = 0; i < [items count]; i++) {
    NSInteger row = [outline_ rowForItem:[items objectAtIndex:i]];
    if (row >= 0) {
      [newSelection addIndex:row];
    }
  }

  [outline_ selectRowIndexes:newSelection byExtendingSelection:NO];
}

// Returns the selected/right-clicked item(s) for a command to act on.
- (NSArray*)actionItems {
  int row = [outline_ clickedRow];
  if (row >= 0 && ![outline_ isRowSelected:row])
    return [NSArray arrayWithObject:[outline_ itemAtRow:row]];

  return [self selectedItems];
}


#pragma mark -
#pragma mark COMMANDS:


// Responds to a double-click by opening the selected URL(s).
- (IBAction)openItems:(id)sender {
  for (BookmarkItem* item in [self actionItems]) {
    if ([outline_ isExpandable:item]) {
      if ([outline_ isItemExpanded:item])
        [outline_ collapseItem:item];
      else
        [outline_ expandItem:item];
    } else {
      [item open];
    }
  }
}

// The Delete command (also bound to the delete key.)
- (IBAction)delete:(id)sender {
  NSArray* items = [self actionItems];
  // Iterate backwards so that any selected children are deleted before
  // selected parents (opposite order could cause double-free!)
  bool any = false;
  if ([items count]) {
    for (NSInteger i = [items count] - 1; i >= 0; i--) {
      BookmarkItem* item = [items objectAtIndex:i];
      if ([[item parent] removeChild:item])
        any = true;
    }
  }
  if (any) {
    [outline_ reloadData];
    [outline_ deselectAll:self];
  } else {
    NSBeep();
  }
}

- (void)editTitleOfItem:(BookmarkItem*)item {
  int row = [outline_ rowForItem:item];
  DCHECK(row >= 0);
  [outline_ editColumn:[outline_ columnWithIdentifier:@"title"]
                   row:row
             withEvent:[NSApp currentEvent]
                select:YES];
}

- (IBAction)editTitle:(id)sender {
  NSArray* items = [self actionItems];
  if ([items count] == 1)
    [self editTitleOfItem:[items objectAtIndex:0]];
  else
    NSBeep();
}

- (IBAction)newFolder:(id)sender {
  BookmarkItem* parent;
  int index;
  NSArray* selItems = [self actionItems];
  if ([selItems count] > 0) {
    // Insert at selected/clicked row.
    BookmarkItem* sel = [selItems objectAtIndex:0];
    parent = [sel parent];
    index = [parent indexOfChild:sel];
  } else {
    // ...or at very end if there's no selection:
    parent = group_;
    index = [group_ numberOfChildren];
  }
  if (!parent) {
      NSBeep();
    return;
  }

  // Create the folder, then select it and make the title editable:
  BookmarkItem* folder = [parent addFolderWithTitle:@"" atIndex:index];
  [outline_ expandItem:folder];
  [self setSelectedItems:[NSArray arrayWithObject:folder]];
  [self editTitleOfItem:folder];
}

- (NSMenu*)menu {
  NSMenu* menu = [manager_ contextMenu];
  for (NSMenuItem* item in [menu itemArray])
    [item setTarget:self];
  return menu;
}

// Selectively enables/disables menu commands.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  NSArray* sel = [self actionItems];
  NSUInteger selCount = [sel count];
  if (action == @selector(copy:)) {
    return selCount > 0;

  } else if (action == @selector(cut:) || action == @selector(delete:)) {
    if (selCount == 0)
      return NO;
    for(BookmarkItem* item in sel) {
      if ([item isFixed])
        return NO;
    }
    return YES;

  } else if (action == @selector(paste:)) {
    return [[NSPasteboard generalPasteboard] availableTypeFromArray:
            [outline_ registeredDraggedTypes]] != nil;

  } else if (action == @selector(openItems:)) {
    if (selCount == 0)
      return NO;
    // Disable if folder selected (if only because title says "Open In Tab".)
    for (BookmarkItem* item in sel) {
      if (![item URLString])
        return NO;
    }
    return YES;

  } else if (action == @selector(editTitle:)) {
    return selCount == 1 && ![[sel lastObject] isFixed];

  } else {
    return YES;
  }
}


#pragma mark -
#pragma mark DATA SOURCE:


// Returns the number of children of an item (NSOutlineView data source)
- (NSInteger)  outlineView:(NSOutlineView*)outlineView
    numberOfChildrenOfItem:(id)item {
  item = (item ? item : group_);
  return [item numberOfChildren];
}

// Returns a child of an item (NSOutlineView data source)
- (id)outlineView:(NSOutlineView*)outlineView
            child:(NSInteger)index
           ofItem:(id)item {
  item = (item ? item : group_);
  return [item childAtIndex:index];
}

// Returns whether an item is expandable (NSOutlineView data source)
- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item {
  return [item isFolder];
}

// Returns the value to display in a cell (NSOutlineView data source)
- (id)            outlineView:(NSOutlineView*)outlineView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                       byItem:(id)item {
  item = (item ? item : group_);
  NSString* ident = [tableColumn identifier];
  if ([ident isEqualToString:kTitleColIdent]) {
    return [item title];
  } else if ([ident isEqualToString:kURLColIdent]) {
    return [item URLString];
  } else if ([ident isEqualToString:kFolderColIdent]) {
    return [item folderPath];
  } else {
    NOTREACHED();
    return nil;
  }
}

// Stores the edited value of a cell (NSOutlineView data source)
- (void)outlineView:(NSOutlineView*)outlineView
     setObjectValue:(id)value
     forTableColumn:(NSTableColumn*)tableColumn
             byItem:(id)item
{
  item = (item ? item : group_);
  NSString* ident = [tableColumn identifier];
  if ([ident isEqualToString:kTitleColIdent]) {
    [item setTitle:value];
  } else if ([ident isEqualToString:kURLColIdent]) {
    [item setURLString:value];
  }
}

// Returns whether a cell is editable (NSOutlineView data source)
- (BOOL)      outlineView:(NSOutlineView*)outlineView
    shouldEditTableColumn:(NSTableColumn*)tableColumn
                     item:(id)item {
  //TODO(snej): Make URL column editable once setter method exists (bug 10603).
  NSString* ident = [tableColumn identifier];
  return [ident isEqualToString:kTitleColIdent] && ![item isFixed];
}

// Sets a cell's icon before it's drawn (NSOutlineView data source)
- (void)outlineView:(NSOutlineView*)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn*)tableColumn
               item:(id)item
{
  // Use the bookmark/folder's icon.
  if ([[tableColumn identifier] isEqualToString:kTitleColIdent]) {
    item = (item ? item : group_);
    [cell setImage:[item icon]];
  }

  // Show special folders (Bookmarks Bar, Others, Recents, Search) in bold.
  static NSFont* sBoldFont = [[NSFont boldSystemFontOfSize:
      [NSFont smallSystemFontSize]] retain];
  static NSFont* sPlainFont = [[NSFont systemFontOfSize:
      [NSFont smallSystemFontSize]] retain];
  [cell setFont:[item isFixed] ? sBoldFont : sPlainFont];
}

@end


@implementation BookmarksOutlineView

- (BookmarkTreeController*)bookmarkController {
  return (BookmarkTreeController*)[self delegate];
}

- (IBAction)cut:(id)sender {
  [[self bookmarkController] cut:sender];
}

- (IBAction)copy:(id)sender {
  [[self bookmarkController] copy:sender];
}

- (IBAction)paste:(id)sender {
  [[self bookmarkController] paste:sender];
}

- (IBAction)delete:(id)sender {
  [[self bookmarkController] delete:sender];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return [[self bookmarkController] validateUserInterfaceItem:item];
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
        [[self bookmarkController] editTitle:self];
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
  return [[self bookmarkController] menu];
}

@end
