// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CARD_MULTILINE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CARD_MULTILINE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Item to display a multiline text, presented in the card style.
@interface CardMultilineItem : TableViewItem

// The text to display.
@property(nonatomic, copy) NSString* text;

@end

@interface CardMultilineCell : UITableViewCell

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CARD_MULTILINE_ITEM_H_
