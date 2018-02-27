// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewTextItem contains the model data for a TableViewTextCell.
@interface TableViewTextItem : TableViewItem

@property(nonatomic, readwrite, strong) NSString* text;

@end

// UITableViewCell that displays a text label.
@interface TableViewTextCell : UITableViewCell

// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
