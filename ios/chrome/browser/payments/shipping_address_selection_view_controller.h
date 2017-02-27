// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

@class ShippingAddressSelectionViewController;

// Delegate protocol for ShippingAddressSelectionViewController.
@protocol ShippingAddressSelectionViewControllerDelegate<NSObject>

// Notifies the delegate that the user has selected a shipping address.
- (void)shippingAddressSelectionViewController:
            (ShippingAddressSelectionViewController*)controller
                      didSelectShippingAddress:
                          (autofill::AutofillProfile*)shippingAddress;

// Notifies the delegate that the user has chosen to return to the previous
// screen without making a selection.
- (void)shippingAddressSelectionViewControllerDidReturn:
    (ShippingAddressSelectionViewController*)controller;

@end

// View controller responsible for presenting the available shipping addresses
// for selection by the user and communicating their choice to the supplied
// delegate. Also offers a button to add a shipping address.
@interface ShippingAddressSelectionViewController : CollectionViewController

// Whether or not the view is in a pending state.
@property(nonatomic, assign, getter=isPending) BOOL pending;

// The error message to display, if any.
@property(nonatomic, copy) NSString* errorMessage;

// The delegate to be notified when the user selects a shipping address or
// returns without selecting one.
@property(nonatomic, weak)
    id<ShippingAddressSelectionViewControllerDelegate> delegate;

// Initializes this object with an instance of PaymentRequest which owns an
// instance of web::PaymentRequest as provided by the page invoking the Payment
// Request API. This object will not take ownership of |paymentRequest|.
- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_H_
