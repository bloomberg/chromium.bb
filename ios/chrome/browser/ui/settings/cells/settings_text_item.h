// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Collection view item to represent and configure a CollectionViewTextCell.
// TODO(crbug.com/894800): Remove this.
@interface SettingsTextItem : CollectionViewItem

// The accessory type for the represented cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

// The main text to display.
@property(nonatomic, nullable, copy) NSString* text;

// The secondary text to display.
@property(nonatomic, nullable, copy) NSString* detailText;

// The font of the main text. Default is the medium SF font of size 16.
@property(nonatomic, null_resettable, copy) UIFont* textFont;

// The color of the main text. Default is the 900 tint color of the grey
// palette.
@property(nonatomic, null_resettable, copy) UIColor* textColor;

@end

// MDCCollectionViewCell that displays two text fields.
@interface SettingsTextCell : MDCCollectionViewCell

// The first line of text to display.
@property(nonatomic, readonly, strong, nullable) UILabel* textLabel;

// The second line of detail text to display.
@property(nonatomic, readonly, strong, nullable) UILabel* detailTextLabel;

// Returns the height needed for a cell contained in |width| to display
// |titleLabel| and |detailTextLabel|.
+ (CGFloat)heightForTitleLabel:(nullable UILabel*)titleLabel
               detailTextLabel:(nullable UILabel*)detailTextLabel
                         width:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_TEXT_ITEM_H_
