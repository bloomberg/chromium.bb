// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class CreditCard;
}

@class PaymentMethodSelectionViewController;

// Delegate protocol for PaymentMethodSelectionViewController.
@protocol PaymentMethodSelectionViewControllerDelegate<NSObject>

// Notifies the delegate that the user has selected a payment method.
- (void)paymentMethodSelectionViewController:
            (PaymentMethodSelectionViewController*)controller
                      didSelectPaymentMethod:
                          (autofill::CreditCard*)paymentMethod;

// Notifies the delegate that the user has chosen to return to the previous
// screen without making a selection.
- (void)paymentMethodSelectionViewControllerDidReturn:
    (PaymentMethodSelectionViewController*)controller;

@end

// View controller responsible for presenting the available payment methods for
// selection by the user and communicating their choice to the supplied
// delegate. Also offers a button to add a new payment method.
@interface PaymentMethodSelectionViewController : CollectionViewController

// The delegate to be notified when the user selects a payment method or chooses
// to return without selecting one.
@property(nonatomic, weak)
    id<PaymentMethodSelectionViewControllerDelegate> delegate;

// Initializes this object with an instance of PaymentRequest which owns an
// instance of web::PaymentRequest as provided by the page invoking the Payment
// Request API. This object will not take ownership of |paymentRequest|.
- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_VIEW_CONTROLLER_H_
