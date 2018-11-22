// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewTextItem contains the model data for a TableViewTextCell.
@interface TableViewTextItem : TableViewItem

// Text Alignment for the cell's textLabel. Default is NSTextAlignmentLeft.
@property(nonatomic, assign) NSTextAlignment textAlignment;

// UIColor for the cell's textLabel. Default is
// kTableViewTextLabelColorLightGrey. ChromeTableViewStyler's |cellTitleColor|
// takes precedence over the default color, but not over |textColor|.
@property(nonatomic, assign) UIColor* textColor;

@property(nonatomic, strong) NSString* text;

// If set to YES, |text| will be shown as "••••••" with fixed length.
@property(nonatomic, assign) BOOL masked;

@end

// UITableViewCell that displays a text label.
@interface TableViewTextCell : UITableViewCell

// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Whether to show the checkmark accessory view.
@property(nonatomic, assign) BOOL checked;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
