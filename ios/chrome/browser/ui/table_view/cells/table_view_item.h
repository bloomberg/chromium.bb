// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/list_model/list_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"

@class ChromeTableViewStyler;

// TableViewItem holds the model data for a given table view item.
@interface TableViewItem : ListItem

// The accessory type to display on the trailing edge of the cell.
@property(nonatomic, assign) UITableViewCellAccessoryType accessoryType;

// Whether custom separator should be used. The separator can replace the
// separator provided by UITableViewCell. It is a 0.5pt high line. Default is
// NO.
@property(nonatomic, assign) BOOL useCustomSeparator;

- (instancetype)initWithType:(NSInteger)type NS_DESIGNATED_INITIALIZER;

// Configures the given cell with the item's information. Override this method
// to specialize. At this level, only accessibility properties are ported from
// the item to the cell.
// The cell's class must match cellClass for the given instance.
- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ITEM_H_
