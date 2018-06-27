// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// SettingsSwitchItem is a model class that uses
// SettingsSwitchCell.
@interface SettingsSwitchItem : CollectionViewItem

// The text to display.
@property(nonatomic, copy) NSString* text;

// The current state of the switch.
@property(nonatomic, assign, getter=isOn) BOOL on;

// Whether or not the switch is enabled.  Disabled switches are automatically
// drawn as in the "off" state, with dimmed text.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// SettingsSwitchCell implements a UICollectionViewCell subclass
// containing an icon, a text label, and a switch.
@interface SettingsSwitchCell : MDCCollectionViewCell

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The switch view.
@property(nonatomic, readonly, strong) UISwitch* switchView;

// Returns the default text color used for the given |state|.
+ (UIColor*)defaultTextColorForState:(UIControlState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_ITEM_H_
