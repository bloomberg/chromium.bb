// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_CELL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"

// SettingsSwitchCell implements a TableViewCell subclass containing an icon,
// a text label, a detail text and a switch.
// If the preferred content size category is an accessibility category, the
// switch is displayed below the label. Otherwise, it is on the trailing side.
@interface SettingsSwitchCell : TableViewCell

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// UILabel corresponding to |detailText| from the item.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// The switch view.
@property(nonatomic, readonly, strong) UISwitch* switchView;

// Returns the default text color used for the given |state|.
+ (UIColor*)defaultTextColorForState:(UIControlState)state;

// Sets the image that should be displayed at the leading edge of the cell. If
// set to nil, the icon will be hidden and the remaining content will expand to
// fill the full width of the cell.
- (void)setIconImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_CELL_H_
