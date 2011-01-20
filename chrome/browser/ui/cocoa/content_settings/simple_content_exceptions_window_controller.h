// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/remove_rows_table_model.h"
#import "chrome/browser/ui/cocoa/table_model_array_controller.h"

class RemoveRowsObserverBridge;

// Controller for the geolocation exception dialog.
@interface SimpleContentExceptionsWindowController : NSWindowController
                                                     <NSWindowDelegate> {
 @private
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* removeButton_;
  IBOutlet NSButton* removeAllButton_;
  IBOutlet NSButton* doneButton_;
  IBOutlet TableModelArrayController* arrayController_;

  scoped_ptr<RemoveRowsTableModel> model_;
}

// Shows or makes frontmost the exceptions window.
// Changes made by the user in the window are persisted in |model|.
// Takes ownership of |model|.
+ (id)controllerWithTableModel:(RemoveRowsTableModel*)model;

// Sets the minimum width of the sheet and resizes it if necessary.
- (void)setMinWidth:(CGFloat)minWidth;

- (void)attachSheetTo:(NSWindow*)window;
- (IBAction)closeSheet:(id)sender;

@end
