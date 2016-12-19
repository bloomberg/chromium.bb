// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_CELLS_SHIPPING_ADDRESS_ITEM_H_
#define IOS_CHROME_BROWSER_PAYMENTS_CELLS_SHIPPING_ADDRESS_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// The model item for shipping address view.
@interface ShippingAddressItem : CollectionViewItem

@property(nonatomic, copy) NSString* name;
@property(nonatomic, copy) NSString* address;
@property(nonatomic, copy) NSString* phoneNumber;

// The accessory type for the represented cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

@end

// The cell for shipping address view with a name, address, and phone number
// text labels.
@interface ShippingAddressCell : MDCCollectionViewCell

@property(nonatomic, readonly, strong) UILabel* nameLabel;
@property(nonatomic, readonly, strong) UILabel* addressLabel;
@property(nonatomic, readonly, strong) UILabel* phoneNumberLabel;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_CELLS_SHIPPING_ADDRESS_ITEM_H_
