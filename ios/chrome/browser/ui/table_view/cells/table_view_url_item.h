// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewURLItem contains the model data for a TableViewURLCell.
@interface TableViewURLItem : TableViewItem

// Sets the faviconView in the cell. If nil, will use the default favicon.
@property(nonatomic, readwrite, strong) UIImage* favicon;
@property(nonatomic, readwrite, copy) NSString* title;
@property(nonatomic, readwrite, copy) NSString* URL;
@property(nonatomic, readwrite, copy) NSString* metadata;

@end

// TableViewURLCell is used in Bookmarks, Reading List, and Recent Tabs.  It
// contains a favicon, a title, a URL, and optionally some metadata such as a
// timestamp or a file size.
@interface TableViewURLCell : UITableViewCell

// The imageview that is displayed on the leading edge of the cell.  This
// contains a favicon composited on top of an off-white background.
@property(nonatomic, readonly, strong) UIImageView* faviconView;

// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;

// The URL associated with this cell.
@property(nonatomic, readonly, strong) UILabel* URLLabel;

// Optional metadata that is displayed at the trailing edge of the cell.
@property(nonatomic, readonly, strong) UILabel* metadataLabel;

// Sets the faviconView image. If nil passed, then default favicon image used.
- (void)setFavicon:(UIImage*)favicon;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
