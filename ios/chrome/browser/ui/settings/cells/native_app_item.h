// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_NATIVE_APP_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_NATIVE_APP_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class MDCButton;

typedef NS_ENUM(NSInteger, NativeAppItemState) {
  NativeAppItemSwitchOff,
  NativeAppItemSwitchOn,
  NativeAppItemInstall,
};

// Item to display and interact with a Native App.
@interface NativeAppItem : CollectionViewItem

// The display name of the app.
@property(nonatomic, copy) NSString* name;

// The icon of the app.
@property(nonatomic, strong) UIImage* icon;

// The state of the item:
// - NativeAppItemSwitchOff: displays a switch set to OFF;
// - NativeAppItemSwitchOn: displays a switch set to ON;
// - NativeAppItemInstall: displays an INSTALL button;
@property(nonatomic, assign) NativeAppItemState state;

@end

// Cell class associated to NativeAppItem.
@interface NativeAppCell : MDCCollectionViewCell

// Label for the app name.
@property(nonatomic, strong, readonly) UILabel* nameLabel;

// Image view for the app icon.
@property(nonatomic, strong, readonly) UIImageView* iconImageView;

// Accessory controls. switchControl is available after calling
// -updateWithState: with NativeAppItemSwitch{On|Off}, while installButton is
// available after calling -updateWithState: with NativeAppItemInstall.
@property(nonatomic, strong, readonly) UISwitch* switchControl;
@property(nonatomic, strong, readonly) MDCButton* installButton;

// Updates the accessory view according to the given |state|.
- (void)updateWithState:(NativeAppItemState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_NATIVE_APP_ITEM_H_
