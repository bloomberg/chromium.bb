// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/table_model_array_controller.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/remove_rows_table_model.h"
#include "ui/base/models/table_model.h"

@interface TableModelArrayController ()

- (NSUInteger)offsetForGroupID:(int)groupID;
- (NSUInteger)offsetForGroupID:(int)groupID startingOffset:(NSUInteger)offset;
- (NSIndexSet*)controllerRowsForModelRowsInRange:(NSRange)range;
- (void)setModelRows:(RemoveRowsTableModel::Rows*)modelRows
    fromControllerRows:(NSIndexSet*)rows;
- (void)modelDidChange;
- (void)modelDidAddItemsInRange:(NSRange)range;
- (void)modelDidRemoveItemsInRange:(NSRange)range;
- (NSDictionary*)columnValuesForRow:(NSInteger)row;

@end

// Observer for a RemoveRowsTableModel.
class RemoveRowsObserverBridge : public ui::TableModelObserver {
 public:
  RemoveRowsObserverBridge(TableModelArrayController* controller)
      : controller_(controller) {}
  virtual ~RemoveRowsObserverBridge() {}

  // ui::TableModelObserver methods
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  TableModelArrayController* controller_;  // weak
};

void RemoveRowsObserverBridge::OnModelChanged() {
  [controller_ modelDidChange];
}

void RemoveRowsObserverBridge::OnItemsChanged(int start, int length) {
  OnItemsRemoved(start, length);
  OnItemsAdded(start, length);
}

void RemoveRowsObserverBridge::OnItemsAdded(int start, int length) {
  [controller_ modelDidAddItemsInRange:NSMakeRange(start, length)];
}

void RemoveRowsObserverBridge::OnItemsRemoved(int start, int length) {
  [controller_ modelDidRemoveItemsInRange:NSMakeRange(start, length)];
}

@implementation TableModelArrayController

static NSString* const kIsGroupRow = @"_is_group_row";
static NSString* const kGroupID = @"_group_id";

- (void)bindToTableModel:(RemoveRowsTableModel*)model
             withColumns:(NSDictionary*)columns
        groupTitleColumn:(NSString*)groupTitleColumn {
  model_ = model;
  tableObserver_.reset(new RemoveRowsObserverBridge(self));
  columns_.reset([columns copy]);
  groupTitle_.reset([groupTitleColumn copy]);
  model_->SetObserver(tableObserver_.get());
  [self modelDidChange];
}

- (void)modelDidChange {
  NSIndexSet* indexes = [NSIndexSet indexSetWithIndexesInRange:
      NSMakeRange(0, [[self arrangedObjects] count])];
  [self removeObjectsAtArrangedObjectIndexes:indexes];
  if (model_->HasGroups()) {
    const ui::TableModel::Groups& groups = model_->GetGroups();
    DCHECK(groupTitle_.get());
    for (ui::TableModel::Groups::const_iterator it = groups.begin();
         it != groups.end(); ++it) {
      NSDictionary* group = [NSDictionary dictionaryWithObjectsAndKeys:
          base::SysUTF16ToNSString(it->title), groupTitle_.get(),
          [NSNumber numberWithBool:YES], kIsGroupRow,
          nil];
      [self addObject:group];
    }
  }
  [self modelDidAddItemsInRange:NSMakeRange(0, model_->RowCount())];
}

- (NSUInteger)offsetForGroupID:(int)groupID startingOffset:(NSUInteger)offset {
  const ui::TableModel::Groups& groups = model_->GetGroups();
  DCHECK_GT(offset, 0u);
  for (NSUInteger i = offset - 1; i < groups.size(); ++i) {
    if (groups[i].id == groupID)
      return i + 1;
  }
  NOTREACHED();
  return NSNotFound;
}

- (NSUInteger)offsetForGroupID:(int)groupID {
  return [self offsetForGroupID:groupID startingOffset:1];
}

- (int)groupIDForControllerRow:(NSUInteger)row {
  NSDictionary* values = [[self arrangedObjects] objectAtIndex:row];
  return [[values objectForKey:kGroupID] intValue];
}

- (void)setModelRows:(RemoveRowsTableModel::Rows*)modelRows
    fromControllerRows:(NSIndexSet*)rows {
  if ([rows count] == 0)
    return;

  if (!model_->HasGroups()) {
    for (NSUInteger i = [rows firstIndex];
         i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
      modelRows->insert(i);
    }
    return;
  }

  NSUInteger offset = 1;
  for (NSUInteger i = [rows firstIndex];
       i != NSNotFound;
       i = [rows indexGreaterThanIndex:i]) {
    int group = [self groupIDForControllerRow:i];
    offset = [self offsetForGroupID:group startingOffset:offset];
    modelRows->insert(i - offset);
  }
}

- (NSIndexSet*)controllerRowsForModelRowsInRange:(NSRange)range {
  if (!model_->HasGroups())
    return [NSIndexSet indexSetWithIndexesInRange:range];
  NSMutableIndexSet* indexes = [NSMutableIndexSet indexSet];
  NSUInteger offset = 1;
  for (NSUInteger i = range.location; i < NSMaxRange(range); ++i) {
    int group = model_->GetGroupID(i);
    offset = [self offsetForGroupID:group startingOffset:offset];
    [indexes addIndex:i + offset];
  }
  return indexes;
}

- (void)modelDidAddItemsInRange:(NSRange)range {
  if (range.length == 0)
    return;
  NSMutableArray* rows = [NSMutableArray arrayWithCapacity:range.length];
  for (NSUInteger i = range.location; i < NSMaxRange(range); ++i)
    [rows addObject:[self columnValuesForRow:i]];
  NSIndexSet* indexes = [self controllerRowsForModelRowsInRange:range];
  [self insertObjects:rows atArrangedObjectIndexes:indexes];
}

- (void)modelDidRemoveItemsInRange:(NSRange)range {
  if (range.length == 0)
    return;
  NSMutableIndexSet* indexes =
      [NSMutableIndexSet indexSetWithIndexesInRange:range];
  if (model_->HasGroups()) {
    // When this method is called, the model has already removed items, so
    // accessing items in the model from |range.location| on may not be possible
    // anymore. Therefore we use the item right before that, if it exists.
    NSUInteger offset = 0;
    if (range.location > 0) {
      int last_group = model_->GetGroupID(range.location - 1);
      offset = [self offsetForGroupID:last_group];
    }
    [indexes shiftIndexesStartingAtIndex:0 by:offset];
    for (NSUInteger row = range.location + offset;
         row < NSMaxRange(range) + offset;
         ++row) {
      if ([self tableView:nil isGroupRow:row]) {
        // Skip over group rows.
        [indexes shiftIndexesStartingAtIndex:row by:1];
        offset++;
      }
    }
  }
  [self removeObjectsAtArrangedObjectIndexes:indexes];
}

- (NSDictionary*)columnValuesForRow:(NSInteger)row {
  NSMutableDictionary* dict = [NSMutableDictionary dictionary];
  if (model_->HasGroups()) {
    [dict setObject:[NSNumber numberWithInt:model_->GetGroupID(row)]
             forKey:kGroupID];
  }
  for (NSString* identifier in columns_.get()) {
    int column_id = [[columns_ objectForKey:identifier] intValue];
    string16 text = model_->GetText(row, column_id);
    [dict setObject:base::SysUTF16ToNSString(text) forKey:identifier];
  }
  return dict;
}

#pragma mark Overridden from NSArrayController

- (BOOL)canRemove {
  if (!model_)
    return NO;
  RemoveRowsTableModel::Rows rows;
  [self setModelRows:&rows fromControllerRows:[self selectionIndexes]];
  return model_->CanRemoveRows(rows);
}

- (IBAction)remove:(id)sender {
  RemoveRowsTableModel::Rows rows;
  [self setModelRows:&rows fromControllerRows:[self selectionIndexes]];
  model_->RemoveRows(rows);
}

#pragma mark NSTableView delegate methods

- (BOOL)tableView:(NSTableView*)tableView isGroupRow:(NSInteger)row {
  NSDictionary* values = [[self arrangedObjects] objectAtIndex:row];
  return [[values objectForKey:kIsGroupRow] boolValue];
}

- (NSIndexSet*)tableView:(NSTableView*)tableView
    selectionIndexesForProposedSelection:(NSIndexSet*)proposedIndexes {
  NSMutableIndexSet* indexes = [proposedIndexes mutableCopy];
  for (NSUInteger i = [proposedIndexes firstIndex];
       i != NSNotFound;
       i = [proposedIndexes indexGreaterThanIndex:i]) {
    if ([self tableView:tableView isGroupRow:i]) {
      [indexes removeIndex:i];
      NSUInteger row = i + 1;
      while (row < [[self arrangedObjects] count] &&
             ![self tableView:tableView isGroupRow:row])
        [indexes addIndex:row++];
    }
  }
  return indexes;
}

#pragma mark Actions

- (IBAction)removeAll:(id)sender {
  model_->RemoveAll();
}

@end

