// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

@class ShippingAddressSelectionViewController;

@protocol ShippingAddressSelectionViewControllerDelegate<NSObject>

- (void)shippingAddressSelectionViewController:
            (ShippingAddressSelectionViewController*)controller
                       selectedShippingAddress:
                           (autofill::AutofillProfile*)shippingAddress;
- (void)shippingAddressSelectionViewControllerDidReturn:
    (ShippingAddressSelectionViewController*)controller;

@end

// View controller responsible for presenting the available shipping addresses
// for selection by the user and communicating their choice to the supplied
// delegate. Also offers a button to add a shipping address.
@interface ShippingAddressSelectionViewController : CollectionViewController

// The available shipping addresses to fulfill the payment request.
@property(nonatomic, assign) std::vector<autofill::AutofillProfile*>
    shippingAddresses;

// The shipping address selected by the user, if any.
@property(nonatomic, assign) autofill::AutofillProfile* selectedShippingAddress;

// The delegate to be notified when the user selects a shipping address or
// returns without selecting one.
@property(nonatomic, weak) id<ShippingAddressSelectionViewControllerDelegate>
    delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_H_
