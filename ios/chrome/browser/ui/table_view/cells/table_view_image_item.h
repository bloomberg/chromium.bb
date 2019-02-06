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
// UIColor for the cell's textLabel. ChromeTableViewStyler's |cellTitleColor|
// takes precedence over the default color, but not over |textColor|.
@property(nonatomic, strong) UIColor* textColor;
// Whether the item is enabled. When it is not enabled, the associated cell
// cannot be interacted with.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// TableViewImageCell contains a favicon, a title, and an optional chevron.
@interface TableViewImageCell : UITableViewCell

// The cell favicon imageView.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_IMAGE_ITEM_H_
