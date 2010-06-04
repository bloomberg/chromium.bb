// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "tab_view_picker_table.h"

#include "base/logging.h"

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

  NSInteger index =
      [tabView_ indexOfTabViewItem:[tabView_ selectedTabViewItem]];
  [self reloadData];
  [self selectRowIndexes:[NSIndexSet indexSetWithIndex:index]
      byExtendingSelection:NO];

  oldTabViewDelegate_ = oldTabViewDelegate;
  [tabView_ setDelegate:self];
}

// Table view delegate method.
- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [tabView_ selectTabViewItemAtIndex:[self selectedRow]];
}

// Table view data source methods.
- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  return [tabView_ numberOfTabViewItems];
}

- (id)              tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  return [[tabView_ tabViewItemAtIndex:rowIndex] label];
}

// NSTabViewDelegate methods.
- (void)         tabView:(NSTabView*)tabView
    didSelectTabViewItem:(NSTabViewItem*)tabViewItem {
  DCHECK_EQ(tabView_, tabView);
  NSInteger index =
      [tabView_ indexOfTabViewItem:[tabView_ selectedTabViewItem]];
  [self selectRowIndexes:[NSIndexSet indexSetWithIndex:index]
      byExtendingSelection:NO];
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

@end
