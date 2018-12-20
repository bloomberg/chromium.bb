// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_IMAGE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_IMAGE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewImageItem contains the model data for a TableViewImageCell.
@interface TableViewImageItem : TableViewItem

// The image in the cell. If nil, won't be added to the view hierarchy.
@property(nonatomic, readwrite, strong) UIImage* image;
// The title label in the cell.
@property(nonatomic, readwrite, copy) NSString* title;
// The accessibility identifier for the cell.
@property(nonatomic, readwrite, copy) NSString* cellAccessibilityIdentifier;
// If YES the cell's chevron will be hidden.
@property(nonatomic, readwrite, assign) BOOL hideChevron;
// UIColor for the cell's textLabel. ChromeTableViewStyler's |cellTitleColor|
// takes precedence over the default color, but not over |textColor|.
@property(nonatomic, strong) UIColor* textColor;

@end

// TableViewImageCell contains a favicon, a title, and an optional chevron.
@interface TableViewImageCell : UITableViewCell

// The cell favicon imageView.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;
// The chevron ImageView.
@property(nonatomic, readonly, strong) UIImageView* chevronImageView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_IMAGE_ITEM_H_
