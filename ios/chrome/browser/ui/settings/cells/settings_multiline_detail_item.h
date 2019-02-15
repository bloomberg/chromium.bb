// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_MULTILINE_DETAIL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_MULTILINE_DETAIL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// SettingsMultilineDetailItem is a model class that uses
// SettingsMultilineDetailCell.
@interface SettingsMultilineDetailItem : TableViewItem

// The main text string.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

@end

// SettingsMultilineDetailCell implements an TableViewCell
// subclass containing two text labels: a "main" label and a "detail" label.
// The two labels are laid out on top of each other and can span on multiple
// lines. This is to be used with a SettingsMultilineDetailItem.
@interface SettingsMultilineDetailCell : TableViewCell

// UILabels corresponding to |text| and |detailText| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_MULTILINE_DETAIL_ITEM_H_
