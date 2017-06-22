// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_PAYMENTS_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_PAYMENTS_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_is_selectable.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// PaymentsTextItem is the model class corresponding to PaymentsTextCell.
@interface PaymentsTextItem : CollectionViewItem<PaymentsIsSelectable>

// The main text to display.
@property(nonatomic, copy) NSString* text;

// The secondary text to display.
@property(nonatomic, copy) NSString* detailText;

// The image to display.
@property(nonatomic, strong) UIImage* image;

// The accessory type for the represented cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

@end

// PaymentsTextCell implements a MDCCollectionViewCell subclass containing
// a main text label, a secondary text label and an optional image. The labels
// are laid out to fill the full width of the cell and are wrapped as needed to
// fit in the cell. The image is laid out on the leading edge of the cell. The
// text labels are laid out on the the trailing edge of the image, if one
// exists, or the leading edge of the cell otherwise, up to the trailing edge of
// the cell.
@interface PaymentsTextCell : MDCCollectionViewCell

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// UILabel corresponding to |detailText| from the item.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// UIImageView corresponding to |image| from the item.
@property(nonatomic, readonly, strong) UIImageView* imageView;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CELLS_PAYMENTS_TEXT_ITEM_H_
