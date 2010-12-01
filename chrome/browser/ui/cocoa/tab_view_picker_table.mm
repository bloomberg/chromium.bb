// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "tab_view_picker_table.h"

#include "base/logging.h"

@interface TabViewPickerTable (Private)
// If a heading is shown, the indices between the tab items and the table rows
// are shifted by one. These functions convert between tab indices and table
// indices.
- (NSInteger)tabIndexFromTableIndex:(NSInteger)tableIndex;
- (NSInteger)tableIndexFromTabIndex:(NSInteger)tabIndex;

// Returns if |item| is the item shown as heading. If |heading_| is nil, this
// always returns |NO|.
- (BOOL)isHeadingItem:(id)item;

// Reloads the outline view and sets the selection to the row corresponding to
// the currently selected tab.
- (void)reloadDataWhileKeepingCurrentTabSelected;
@end

@implementation TabViewPickerTable

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setDelegate:self];
    [self setDataSource:self];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)coder {
  if ((self = [super initWithCoder:coder])) {
    [self setDelegate:self];
    [self setDataSource:self];
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK(tabView_);
  DCHECK_EQ([self delegate], self);
  DCHECK_EQ([self dataSource], self);
  DCHECK(![self allowsEmptySelection]);
  DCHECK(![self allowsMultipleSelection]);

  // Suppress the "Selection changed" message that's sent while the table is
  // being built for the first time (this causes a selection change to index 0
  // and back to the prior index).
  id oldTabViewDelegate = [tabView_ delegate];
  [tabView_ setDelegate:nil];

  [self reloadDataWhileKeepingCurrentTabSelected];

  oldTabViewDelegate_ = oldTabViewDelegate;
  [tabView_ setDelegate:self];
}

- (NSString*)heading {
  return heading_.get();
}

- (void)setHeading:(NSString*)str {
  heading_.reset([str copy]);
  [self reloadDataWhileKeepingCurrentTabSelected];
}

- (void)reloadDataWhileKeepingCurrentTabSelected {
  NSInteger index =
      [tabView_ indexOfTabViewItem:[tabView_ selectedTabViewItem]];
  [self reloadData];
  if (heading_)
    [self expandItem:[self outlineView:self child:0 ofItem:nil]];
  NSIndexSet* indexSet =
      [NSIndexSet indexSetWithIndex:[self tableIndexFromTabIndex:index]];
  [self selectRowIndexes:indexSet byExtendingSelection:NO];
}

// NSTabViewDelegate methods.
- (void)         tabView:(NSTabView*)tabView
    didSelectTabViewItem:(NSTabViewItem*)tabViewItem {
  DCHECK_EQ(tabView_, tabView);
  NSInteger index =
      [tabView_ indexOfTabViewItem:[tabView_ selectedTabViewItem]];
  NSIndexSet* indexSet =
      [NSIndexSet indexSetWithIndex:[self tableIndexFromTabIndex:index]];
  [self selectRowIndexes:indexSet byExtendingSelection:NO];
  if ([oldTabViewDelegate_
          respondsToSelector:@selector(tabView:didSelectTabViewItem:)]) {
    [oldTabViewDelegate_ tabView:tabView didSelectTabViewItem:tabViewItem];
  }
}

- (BOOL)            tabView:(NSTabView*)tabView
    shouldSelectTabViewItem:(NSTabViewItem*)tabViewItem {
  if ([oldTabViewDelegate_
          respondsToSelector:@selector(tabView:shouldSelectTabViewItem:)]) {
    return [oldTabViewDelegate_ tabView:tabView
                shouldSelectTabViewItem:tabViewItem];
  }
  return YES;
}

- (void)          tabView:(NSTabView*)tabView
    willSelectTabViewItem:(NSTabViewItem*)tabViewItem {
  if ([oldTabViewDelegate_
          respondsToSelector:@selector(tabView:willSelectTabViewItem:)]) {
    [oldTabViewDelegate_ tabView:tabView willSelectTabViewItem:tabViewItem];
  }
}

- (NSInteger)tabIndexFromTableIndex:(NSInteger)tableIndex {
  if (!heading_)
    return tableIndex;
  DCHECK(tableIndex > 0);
  return tableIndex - 1;
}

- (NSInteger)tableIndexFromTabIndex:(NSInteger)tabIndex {
  DCHECK_GE(tabIndex, 0);
  DCHECK_LT(tabIndex, [tabView_ numberOfTabViewItems]);
  if (!heading_)
    return tabIndex;
  return tabIndex + 1;
}

- (BOOL)isHeadingItem:(id)item {
  return item && item == heading_.get();
}

// NSOutlineViewDataSource methods.
- (NSInteger)  outlineView:(NSOutlineView*)outlineView
    numberOfChildrenOfItem:(id)item {
  if (!item)
    return heading_ ? 1 : [tabView_ numberOfTabViewItems];
  return (item == heading_.get()) ? [tabView_ numberOfTabViewItems] : 0;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item {
  return [self isHeadingItem:item];
}

- (id)outlineView:(NSOutlineView*)outlineView
            child:(NSInteger)index
           ofItem:(id)item {
  if (!item) {
    return heading_.get() ?
        heading_.get() : static_cast<id>([tabView_ tabViewItemAtIndex:index]);
  }
  return (item == heading_.get()) ? [tabView_ tabViewItemAtIndex:index] : nil;
}

- (id)            outlineView:(NSOutlineView*)outlineView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                       byItem:(id)item {
  if ([item isKindOfClass:[NSTabViewItem class]])
    return [static_cast<NSTabViewItem*>(item) label];
  if ([self isHeadingItem:item])
    return [item uppercaseString];
  return nil;
}

// NSOutlineViewDelegate methods.
- (void)outlineViewSelectionDidChange:(NSNotification*)notification {
  int row = [self selectedRow];
  [tabView_ selectTabViewItemAtIndex:[self tabIndexFromTableIndex:row]];
}

- (BOOL)outlineView:(NSOutlineView *)sender isGroupItem:(id)item {
  return [self isHeadingItem:item];
}

- (BOOL)outlineView:(NSOutlineView*)outlineView shouldExpandItem:(id)item {
  return [self isHeadingItem:item];
}

- (BOOL)outlineView:(NSOutlineView*)outlineView shouldCollapseItem:(id)item {
  return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item {
  return ![self isHeadingItem:item];
}

// -outlineView:shouldShowOutlineCellForItem: is 10.6-only.
- (NSRect)frameOfOutlineCellAtRow:(NSInteger)row {
  return NSZeroRect;
}

@end
