// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_CELLS_PAYMENT_METHOD_ITEM_H_
#define IOS_CHROME_BROWSER_PAYMENTS_CELLS_PAYMENT_METHOD_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// PaymentMethodItem is the model class corresponding to PaymentMethodCell.
@interface PaymentMethodItem : CollectionViewItem

// A unique identifier for the payment method (for example, the type and last 4
// digits of a credit card).
@property(nonatomic, copy) NSString* methodID;

// Additional details about the payment method (for example, the name of the
// credit card holder).
@property(nonatomic, copy) NSString* methodDetail;

// An image corresponding to the type of the payment method.
@property(nonatomic, strong) UIImage* methodTypeIcon;

// The accessory type to be shown in the cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

@end

// PaymentMethodCell implements an MDCCollectionViewCell subclass containing two
// text labels providing details about a payment method and an image view
// displaying an icon representing the payment method's type. The image is laid
// out on the trailing edge of the cell while the two labels are laid out on the
// leading edge of the cell up to the leading edge of the image view. The labels
// are allowed to break to multiple lines.
@interface PaymentMethodCell : MDCCollectionViewCell

// UILabels containing an identifier and additional details about the payment
// method.
@property(nonatomic, readonly, strong) UILabel* methodIDLabel;
@property(nonatomic, readonly, strong) UILabel* methodDetailLabel;

// UIImageView containing the paymen method type icon.
@property(nonatomic, readonly, strong) UIImageView* methodTypeIconView;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_CELLS_PAYMENT_METHOD_ITEM_H_
