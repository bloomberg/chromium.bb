// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"

// TableViewTextHeaderFooterItem contains the model data for a
// UITableViewHeaderFooterView.
@interface TableViewTextHeaderFooterItem : TableViewHeaderFooterItem
@property(nonatomic, readwrite, strong) NSString* text;
@property(nonatomic, readwrite, strong) NSString* subtitleText;
@end

// UITableViewHeaderFooterView that displays a text label.
@interface TableViewTextHeaderFooterView : UITableViewHeaderFooterView
@property(nonatomic, readwrite, strong) UILabel* subtitleLabel;
// Color used on the highlight animation.
@property(nonatomic, readwrite, strong) UIColor* highlightColor;
// Animates a change in the backgroundView color and then changes it back to the
// original backGround color in order to simulate a selection highlight.
- (void)animateHighlight;
@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_HEADER_FOOTER_ITEM_H_
