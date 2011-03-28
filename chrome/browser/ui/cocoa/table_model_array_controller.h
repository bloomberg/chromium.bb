// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABLE_MODEL_ARRAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TABLE_MODEL_ARRAY_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/models/table_model_observer.h"

class RemoveRowsObserverBridge;
class RemoveRowsTableModel;
@class TableModelArrayController;

// This class allows you to use a RemoveRowsTableModel with Cocoa bindings.
// It maps the CanRemoveRows method to its canRemove property, and exposes
// RemoveRows and RemoveAll as actions (remove: and removeAll:).
// If the table model has groups, these are inserted into the list of arranged
// objects as group rows.
// The designated initializer is the same as for NSArrayController,
// initWithContent:, but usually this class is instantiated from a nib file.
// Clicking on a group row selects all rows belonging to that group, like it
// does in a Windows table_view.
// In order to show group rows, this class must be the delegate of the
// NSTableView.
@interface TableModelArrayController : NSArrayController<NSTableViewDelegate> {
 @private
  RemoveRowsTableModel* model_; // weak
  scoped_ptr<RemoveRowsObserverBridge> tableObserver_;
  scoped_nsobject<NSDictionary> columns_;
  scoped_nsobject<NSString> groupTitle_;
}

// Bind this controller to the given model.
// |columns| is a dictionary mapping table column bindings to NSNumbers
// containing the column identifier in the TableModel.
// |groupTitleColumn| is the column in the table that should display the group
// title for a group row, usually the first column. If the model doesn't have
// groups, it can be nil.
- (void)bindToTableModel:(RemoveRowsTableModel*)model
             withColumns:(NSDictionary*)columns
        groupTitleColumn:(NSString*)groupTitleColumn;

- (IBAction)removeAll:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABLE_MODEL_ARRAY_CONTROLLER_H_

