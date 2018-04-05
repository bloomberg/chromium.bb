// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DISCLOSURE_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DISCLOSURE_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"

// TableViewDisclosureHeaderFooterItem contains the model data for a
// TableViewDisclosureHeaderFooterView.
@interface TableViewDisclosureHeaderFooterItem : TableViewHeaderFooterItem
// Title of Header.
@property(nonatomic, readwrite, strong) NSString* text;
// Header subtitle displayed as a smaller font under title.
@property(nonatomic, readwrite, strong) NSString* subtitleText;
@end

// UITableViewHeaderFooterView that displays a text label, subtitle, and a
// disclosure accessory view.
@interface TableViewDisclosureHeaderFooterView : UITableViewHeaderFooterView
// Shows the text of the TableViewDisclosureHeaderFooterItem.
@property(nonatomic, readwrite, strong) UILabel* titleLabel;
// Shows the subtitleText of the TableViewDisclosureHeaderFooterItem.
@property(nonatomic, readwrite, strong) UILabel* subtitleLabel;
// Animates a change in the backgroundView color and then changes it back to the
// original backGround color in order to simulate a selection highlight.
- (void)animateHighlight;
@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DISCLOSURE_HEADER_FOOTER_ITEM_H_
