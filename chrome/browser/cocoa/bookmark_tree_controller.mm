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

@synthesize showsLeaves = showsLeaves_;

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

- (BOOL)showsFolderColumn {
  NSTableColumn* col = [outline_ tableColumnWithIdentifier:kFolderColIdent];
  return ![col isHidden];
}

- (void)setShowsFolderColumn:(BOOL)show {
  NSTableColumn* col = [outline_ tableColumnWithIdentifier:kFolderColIdent];
  DCHECK(col);
  if (show != ![col isHidden]) {
    [col setHidden:!show];
    [outline_ sizeToFit];
  }
}

- (BOOL)flat {
  return flat_;
}

- (void)setFlat:(BOOL)flat {
  flat_ = flat;
  [outline_ setIndentationPerLevel:flat ? 0.0 : 16.0];
}


#pragma mark -
#pragma mark SELECTION:

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

- (BookmarkItem*)selectedItem {
  BookmarkItem* selected = nil;
  if ([outline_ numberOfSelectedRows] == 1)
    selected = [outline_ itemAtRow:[outline_ selectedRow]];
  return selected;
}

- (void)setSelectedItem:(BookmarkItem*)item {
  [self setSelectedItems:item ? [NSArray arrayWithObject:item] : nil];
}

+ (NSArray*)keyPathsForValuesAffectingSelectedItem {
  return [NSArray arrayWithObject:@"selectedItems"];
}

- (BOOL)selectionShouldChangeInOutlineView:(NSOutlineView*)outlineView {
  [self willChangeValueForKey:@"selectedItems"];
  return YES;
}

- (void)outlineViewSelectionDidChange:(NSNotification*)notification {
  [self didChangeValueForKey:@"selectedItems"];
}

// Returns the selected/right-clicked item(s) for a command to act on.
- (NSArray*)actionItems {
  int row = [outline_ clickedRow];
  if (row >= 0 && ![outline_ isRowSelected:row])
    return [NSArray arrayWithObject:[outline_ itemAtRow:row]];

  return [self selectedItems];
}

- (BOOL)expandItem:(BookmarkItem*)item {
  if (!item)
    return NO;
  if (item == group_)
    return YES;
  if ([outline_ rowForItem:item] < 0) {
    // If the item's not visible, expand its ancestors.
    if (![self expandItem:[item parent]])
      return NO;
  }
  if (![outline_ isExpandable:item])
    return NO;
  [outline_ expandItem:item];
  DCHECK([outline_ isItemExpanded:item]);
  return YES;
}

- (BOOL)revealItem:(BookmarkItem*)item {
  if (![self expandItem:[item parent]])
    return NO;
  [outline_ scrollRowToVisible:[outline_ rowForItem:item]];
  [self setSelectedItems:[NSArray arrayWithObject:item]];
  return YES;
}

- (BOOL)getInsertionParent:(BookmarkItem**)outParent
                     index:(NSUInteger*)outIndex {
  NSArray* selItems = [self actionItems];
  NSUInteger numSelected = [selItems count];
  if (numSelected > 1) {
    return NO;
  } else if (numSelected == 1) {
    // Insert at selected/clicked row.
    BookmarkItem* selected = [selItems objectAtIndex:0];
    *outParent = [selected parent];
    if (*outParent && ![*outParent isFake])
      *outIndex = [*outParent indexOfChild:selected];
    else if ([selected isFolder]) {
      // If root item like Bookmarks Bar selected, insert _into_ it.
      *outParent = selected;
      *outIndex = 0;
    }
  } else {
    // Insert at very end if there's no selection:
    *outParent = group_;
    *outIndex = [group_ numberOfChildren];
  }
  // Can't add to a fake group.
  return *outParent && ![*outParent isFake];
}

- (BOOL)canInsert {
  BookmarkItem* parent;
  NSUInteger index;
  return [self getInsertionParent:&parent index:&index];
}


#pragma mark -
#pragma mark COMMANDS:


// Responds to a double-click by opening the selected URL(s).
- (IBAction)openItems:(id)sender {
  for (BookmarkItem* item in [self actionItems]) {
    if (![item isFolder]) {
      [item open];
    } else if (flat_) {
      [manager_ showGroup:item];
    } else {
      if ([outline_ isItemExpanded:item])
        [outline_ collapseItem:item];
      else
        [outline_ expandItem:item];
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
  [self setSelectedItem:item];
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
  NSUInteger index;
  if (![self getInsertionParent:&parent index:&index]) {
    NSBeep();
    return;
  }
  // Create the folder, then select it and make the title editable:
  BookmarkItem* folder = [parent addFolderWithTitle:@"" atIndex:index];
  [self expandItem:folder];
  [self editTitleOfItem:folder];
}

- (IBAction)revealSelectedItem:(id)sender {
  NSArray* selItems = [self actionItems];
  if ([selItems count] != 1 ||
      ![manager_ revealItem:[selItems objectAtIndex:0]])
    NSBeep();
  [[outline_ window] makeFirstResponder:outline_];
}

static void addItem(NSMenu* menu, int command, SEL action) {
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(command)
                  action:action
           keyEquivalent:@""];
}

// Generates a context menu for the outline view.
- (NSMenu*)menu {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
  addItem(menu, IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB, @selector(openItems:));
  if (showsLeaves_) {
    addItem(menu, IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER,
        @selector(revealSelectedItem:));
  }
  [menu addItem:[NSMenuItem separatorItem]];
  addItem(menu, IDS_BOOKMARK_BAR_EDIT, @selector(editTitle:));
  addItem(menu, IDS_BOOKMARK_BAR_REMOVE, @selector(delete:));
  [menu addItem:[NSMenuItem separatorItem]];
  addItem(menu, IDS_BOOMARK_BAR_NEW_FOLDER, @selector(newFolder:));
  for (NSMenuItem* item in [menu itemArray])
    [item setTarget:self];
  return menu;
}

// Selectively enables/disables menu commands.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return [self validateAction:[item action]];
}

- (BOOL)validateAction:(SEL)action {
  NSArray* selected = [self actionItems];
  NSUInteger selCount = [selected count];
  if (action == @selector(copy:)) {
    return selCount > 0;

  } else if (action == @selector(cut:) || action == @selector(delete:)) {
    if (selCount == 0)
      return NO;
    for(BookmarkItem* item in selected) {
      if ([item isFixed])
        return NO;
    }
    return YES;

  } else if (action == @selector(paste:)) {
    return [self canInsert] &&
        [[NSPasteboard generalPasteboard] availableTypeFromArray:
            [outline_ registeredDraggedTypes]] != nil;

  } else if (action == @selector(openItems:)) {
    if (selCount == 0)
      return NO;
    // Disable if folder selected (if only because title says "Open In Tab".)
    for (BookmarkItem* item in selected) {
      if (![item URLString])
        return NO;
    }
    return YES;

  } else if (action == @selector(editTitle:)) {
    return selCount == 1 && ![[selected lastObject] isFixed];

  } else if (action == @selector(newFolder:)) {
    return [self canInsert];

  } else if (action == @selector(revealSelectedItem:)) {
    // Enable Show In Folder only in the flat list when
    // showing Recents or Search, and a URL is selected.
    return flat_ && [group_ isFake] &&
        selCount == 1 && [[selected lastObject] URLString];

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
  return showsLeaves_ ? [item numberOfChildren] : [item numberOfChildFolders];
}

// Returns a child of an item (NSOutlineView data source)
- (id)outlineView:(NSOutlineView*)outlineView
            child:(NSInteger)index
           ofItem:(id)item {
  item = (item ? item : group_);
  if (showsLeaves_)
    return [item childAtIndex:index];
  else
    return [item childFolderAtIndex:index];
}

// Returns whether an item is expandable (NSOutlineView data source)
- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item {
  if (flat_ || ![(item ? item : group_) isFolder])
    return NO;
  // In leafless mode, a folder with no subfolders isn't expandable.
  if (!showsLeaves_ && [item numberOfChildFolders] == 0)
    return NO;
  return YES;
}

- (id)          outlineView:(NSOutlineView*)outlineView
    itemForPersistentObject:(id)persistentID {
  return [group_ itemWithPersistentID:persistentID];
}

- (id)          outlineView:(NSOutlineView*)outlineView
    persistentObjectForItem:(id)item {
  item = (item ? item : group_);
  return [item persistentID];
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
    if ([value length])
      [item setURLString:value];
  }
}

// Returns whether a cell is editable (NSOutlineView data source)
- (BOOL)      outlineView:(NSOutlineView*)outlineView
    shouldEditTableColumn:(NSTableColumn*)tableColumn
                     item:(id)item {
  NSString* ident = [tableColumn identifier];
  if ([item isFixed])
    return NO;
  if ([ident isEqualToString:kURLColIdent] && [item isFolder])
    return NO;
  return YES;
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
}

// Updates the tree after the data model has changed.
- (void)itemChanged:(id)nodeItem childrenChanged:(BOOL)childrenChanged {
  if (nodeItem == group_)
    nodeItem = nil;
  [outline_ reloadItem:nodeItem reloadChildren:childrenChanged];
}

@end


@implementation BookmarksOutlineView

- (BookmarkTreeController*)bookmarkController {
  return (BookmarkTreeController*)[self delegate];
}

- (NSRect)frameOfOutlineCellAtRow:(NSInteger)row {
  // If the controller is in flat view, don't reserve space for the triangles.
  BookmarkTreeController* controller = [self bookmarkController];
  if ([controller flat])
    return NSZeroRect;
  return [super frameOfOutlineCellAtRow:row];
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
