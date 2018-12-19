// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LEGACY_LEGACY_SYNC_SWITCH_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LEGACY_LEGACY_SYNC_SWITCH_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@interface LegacySyncSwitchItem : CollectionViewItem

// The title text to display.
@property(nonatomic, copy) NSString* text;

// The detail text to display.
@property(nonatomic, copy) NSString* detailText;

// The current state of the switch.
@property(nonatomic, assign, getter=isOn) BOOL on;

// Whether or not the switch is enabled.  Disabled switches are automatically
// drawn as in the "off" state, with dimmed text.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// SyncSetupService::SyncableDatatype value for the item.
@property(nonatomic, assign) NSInteger dataType;

// Command to trigger when the switch is toggled. The default value is 0.
@property(nonatomic, assign) NSInteger commandID;

@end

// Cell representation for AccountSignInItem.
@interface LegacySyncSwitchCell : MDCCollectionViewCell

// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Cell subtitle.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// The switch view.
@property(nonatomic, readonly, strong) UISwitch* switchView;

// Returns the default text color used for the given |state|.
+ (UIColor*)defaultTextColorForState:(UIControlState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LEGACY_LEGACY_SYNC_SWITCH_ITEM_H_
