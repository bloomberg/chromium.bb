// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_ITEMS_DISPLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_ITEMS_DISPLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <vector>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

@class PaymentItemsDisplayViewController;

// Protocol for communicating with the delegate supplied to
// PaymentItemsDisplayViewController.
@protocol PaymentItemsDisplayViewControllerDelegate<NSObject>

// Notifies the delegate that the user has chosen to return to the previous
// screen.
- (void)paymentItemsDisplayViewControllerDidReturn:
    (PaymentItemsDisplayViewController*)controller;

// Notifies the delegate that the user has confirmed the payment.
- (void)paymentItemsDisplayViewControllerDidConfirm:
    (PaymentItemsDisplayViewController*)controller;

@end

// View controller responsible for presenting the payment items from the payment
// request. The payment items are the line items that should sum to the total
// payment (e.g. individual goods/services, tax, shipping).
@interface PaymentItemsDisplayViewController : CollectionViewController

// The payment total to be displayed to the user.
@property(nonatomic, assign) web::PaymentItem total;

// The line items to be displayed to the user.
@property(nonatomic, assign) std::vector<web::PaymentItem> paymentItems;

// The delegate to be notified when the user selects touches the return button
// or the pay button.
@property(nonatomic, weak) id<PaymentItemsDisplayViewControllerDelegate>
    delegate;

- (instancetype)initWithPayButtonEnabled:(BOOL)payButtonEnabled
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_ITEMS_DISPLAY_VIEW_CONTROLLER_H_
