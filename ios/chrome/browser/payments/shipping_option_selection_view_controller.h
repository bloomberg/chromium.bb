// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_OPTION_SELECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_OPTION_SELECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

@class ShippingOptionSelectionViewController;

// Delegate protocol for ShippingOptionSelectionViewController.
@protocol ShippingOptionSelectionViewControllerDelegate<NSObject>

// Notifies the delegate that the user has selected a shipping option.
- (void)shippingOptionSelectionViewController:
            (ShippingOptionSelectionViewController*)controller
                       selectedShippingOption:
                           (web::PaymentShippingOption*)shippingOption;

// Notifies the delegate that the user has chosen to return to the previous
// screen without making a selection.
- (void)shippingOptionSelectionViewControllerDidReturn:
    (ShippingOptionSelectionViewController*)controller;

@end

// View controller responsible for presenting the available shipping options
// for selection by the user and communicating their choice to the supplied
// delegate.
@interface ShippingOptionSelectionViewController : CollectionViewController

// The available shipping options to fulfill the payment request.
@property(nonatomic, assign) std::vector<web::PaymentShippingOption*>
    shippingOptions;

// The shipping option selected by the user, if any.
@property(nonatomic, assign) web::PaymentShippingOption* selectedShippingOption;

// The delegate to be notified when the user selects a shipping option or
// returns without selecting one.
@property(nonatomic, weak) id<ShippingOptionSelectionViewControllerDelegate>
    delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_OPTION_SELECTION_VIEW_CONTROLLER_H_
