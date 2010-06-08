// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"

// TabViewPickerTable is an NSTableView that can be used to switch between the
// NSTabViewItems of an NSTabView. To use this, just create a
// TabViewPickerTable in Interface Builder and connect the |tabView_| outlet
// to an NSTabView. Now the table is automatically populated with the tab labels
// of the tab view, clicking the table updates the tab view, and switching
// tab view items updates the selection of the table.
@interface TabViewPickerTable : NSTableView <NSTabViewDelegate,
                                            NSTableViewDelegate,
                                            NSTableViewDataSource> {
 @public
  IBOutlet NSTabView* tabView_;  // Visible for testing.

 @private
  id oldTabViewDelegate_;
}
@end
