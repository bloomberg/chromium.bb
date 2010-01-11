// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_tree_controller.h"

#include "base/nsimage_cache_mac.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "googleurl/src/gurl.h"
#import "third_party/apple/ImageAndTextCell.h"


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

- (id)group {
  return group_;
}

- (void)setGroup:(id)group {
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
- (void)itemChanged:(id)nodeItem childrenChanged:(BOOL)childrenChanged {
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
  for (id item in [self actionItems]) {
    if ([outline_ isExpandable:item]) {
      if ([outline_ isItemExpanded:item])
        [outline_ collapseItem:item];
      else
        [outline_ expandItem:item];
    } else {
      [manager_ openBookmarkItem:item];
    }
  }
}

// The Delete command (also bound to the delete key.)
- (IBAction)delete:(id)sender {
  NSArray* items = [self actionItems];
  if ([items count] == 0) {
    NSBeep();
    return;
  }
  // Iterate backwards so that any selected children are deleted before
  // selected parents (opposite order would cause double-free!) and so each
  // deletion doesn't invalidate the remaining row numbers.
  for (NSInteger i = [items count] - 1; i >= 0; i--) {
    const BookmarkNode* node = [manager_ nodeFromItem:
        [items objectAtIndex:i]];
    const BookmarkNode* parent = node->GetParent();
    [manager_ bookmarkModel]->Remove(parent, parent->IndexOfChild(node));
  }

  [outline_ reloadData];
  [outline_ deselectAll:self];
}

- (void)editTitleOfItem:(id)item {
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
  const BookmarkNode* targetNode;
  NSInteger childIndex;
  NSArray* items = [self actionItems];
  if ([items count] > 0) {
    // Insert at selected/clicked row.
    const BookmarkNode* selNode = [self nodeFromItem:
                                   [items objectAtIndex:0]];
    targetNode = selNode->GetParent();
    childIndex = targetNode->IndexOfChild(selNode);
  } else {
    // ...or at very end if there's no selection:
    targetNode = [self nodeFromItem:group_];
    childIndex = targetNode->GetChildCount();
  }

  const BookmarkNode* folder = [manager_ bookmarkModel]->
      AddGroup(targetNode, childIndex, L"");
  id folderItem = [manager_ itemFromNode:folder];
  [outline_ expandItem:folderItem];
  [self setSelectedItems:[NSArray arrayWithObject:folderItem]];
  [self editTitleOfItem:folderItem];
}

- (NSMenu*)menu {
  NSMenu* menu = [manager_ contextMenu];
  for (NSMenuItem* item in [menu itemArray])
    [item setTarget:self];
  return menu;
}

// Selectively enables/disables menu commands.
- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  SEL action = [menuItem action];
  if (action == @selector(cut:) || action == @selector(copy:) ||
      action == @selector(delete:)) {
    return [[self actionItems] count] > 0;
  } else if (action == @selector(openItems:)) {
    NSArray* items = [self actionItems];
    if ([items count] == 0)
      return NO;
    // Disable if folder selected (if only because title says "Open In Tab".)
    for (id item in items) {
      const BookmarkNode* node = [manager_ nodeFromItem:item];
      if (!node->is_url())
        return NO;
    }
    return YES;
  } else if (action == @selector(editTitle:)) {
    return [[self actionItems] count] == 1;
  } else if (action == @selector(paste:)) {
    return [[NSPasteboard generalPasteboard]
        availableTypeFromArray:[outline_ registeredDraggedTypes]]
        != nil;
  } else {
    return YES;
  }
}


#pragma mark -
#pragma mark DATA SOURCE:


// The NSOutlineView data source methods are called with a nil item to
// represent the root of the tree; this compensates for that.
- (const BookmarkNode*)nodeFromItem:(id)item {
  return [manager_ nodeFromItem:(item ? item : group_)];
}

- (id)itemFromNode:(const BookmarkNode*)node {
  id item = [manager_ itemFromNode:node];
  return item == group_ ? nil : item;
}

// Returns the children of an item (NSOutlineView data source)
- (NSArray*)childrenOfItem:(id)item {
  const BookmarkNode* node = [self nodeFromItem:item];
  if (!node) {
    return nil;
  }
  int nChildren = node->GetChildCount();
  NSMutableArray* children = [NSMutableArray arrayWithCapacity:nChildren];
  for (int i = 0; i < nChildren; i++) {
    [children addObject:[self itemFromNode:node->GetChild(i)]];
  }
  return children;
}

// Returns the number of children of an item (NSOutlineView data source)
- (NSInteger)  outlineView:(NSOutlineView*)outlineView
    numberOfChildrenOfItem:(id)item {
  const BookmarkNode* node = [self nodeFromItem:item];
  return node ? node->GetChildCount() : 0;
}

// Returns a child of an item (NSOutlineView data source)
- (id)outlineView:(NSOutlineView*)outlineView
            child:(NSInteger)index
           ofItem:(id)item {
  const BookmarkNode* node = [self nodeFromItem:item];
  return [self itemFromNode:node->GetChild(index)];
}

// Returns whether an item is a folder (NSOutlineView data source)
- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item {
  return [self nodeFromItem:item]->is_folder();
}

// Returns the value to display in a cell (NSOutlineView data source)
- (id)            outlineView:(NSOutlineView*)outlineView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                       byItem:(id)item {
  const BookmarkNode* node = [self nodeFromItem:item];
  NSString* ident = [tableColumn identifier];
  if ([ident isEqualToString:@"title"]) {
    return base::SysWideToNSString(node->GetTitle());
  } else if ([ident isEqualToString:@"url"]) {
    return base::SysUTF8ToNSString(node->GetURL().possibly_invalid_spec());
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
  const BookmarkNode* node = [self nodeFromItem:item];
  NSString* ident = [tableColumn identifier];
  if ([ident isEqualToString:@"title"]) {
    [manager_ bookmarkModel]->SetTitle(node, base::SysNSStringToWide(value));
  } else if ([ident isEqualToString:@"url"]) {
    GURL url(base::SysNSStringToUTF8(value));
    if (url != node->GetURL()) {
      //TODO(snej): Uncomment this once SetURL exists (bug 10603).
      //  ...or work around it by removing node and adding new one.
      //[manager_ bookmarkModel]->SetURL(node, url);
    }
  }
}

// Returns whether a cell is editable (NSOutlineView data source)
- (BOOL)      outlineView:(NSOutlineView*)outlineView
    shouldEditTableColumn:(NSTableColumn*)tableColumn
                     item:(id)item {
  //TODO(snej): Make URL column editable once setter method exists (bug 10603).
  NSString* ident = [tableColumn identifier];
  return [ident isEqualToString:@"title"];
}

// Sets a cell's icon before it's drawn (NSOutlineView data source)
- (void)outlineView:(NSOutlineView*)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn*)tableColumn
               item:(id)item
{
  if ([[tableColumn identifier] isEqualToString:@"title"]) {
      [(ImageAndTextCell*)cell setImage:[manager_ iconForItem:item]];
  }
}

@end


@implementation BookmarksOutlineView

- (IBAction)cut:(id)sender {
  [(BookmarkTreeController*)[self delegate] cut:sender];
}

- (IBAction)copy:(id)sender {
  [(BookmarkTreeController*)[self delegate] copy:sender];
}

- (IBAction)paste:(id)sender {
  [(BookmarkTreeController*)[self delegate] paste:sender];
}

- (IBAction)delete:(id)sender {
  [(BookmarkTreeController*)[self delegate] delete:sender];
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  return [(BookmarkTreeController*)[self delegate] validateMenuItem:menuItem];
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
        [(BookmarkTreeController*)[self delegate] editTitle:self];
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
  return [[self delegate] menu];
}

@end
