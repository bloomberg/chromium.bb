// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_COORDINATOR_H_

#import <UIKit/UIKit.h>
#include <vector>

#import "ios/chrome/browser/chrome_coordinator.h"
#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"

namespace autofill {
class CreditCard;
}

@class PaymentMethodSelectionCoordinator;

@protocol PaymentMethodSelectionCoordinatorDelegate<NSObject>

// Notifies the delegate that the user has selected a payment method.
- (void)paymentMethodSelectionCoordinator:
            (PaymentMethodSelectionCoordinator*)coordinator
                    selectedPaymentMethod:(autofill::CreditCard*)paymentMethod;

// Notifies the delegate that the user has chosen to return to the previous
// screen without making a selection.
- (void)paymentMethodSelectionCoordinatorDidReturn:
    (PaymentMethodSelectionCoordinator*)coordinator;

@end

// Coordinator responsible for creating and presenting the payment method
// selection view controller. This view controller will be presented by the view
// controller provided in the initializer.
@interface PaymentMethodSelectionCoordinator
    : ChromeCoordinator<PaymentMethodSelectionViewControllerDelegate>

// The payment methods available to fulfill the payment request.
@property(nonatomic, assign) std::vector<autofill::CreditCard*> paymentMethods;

// The payment method selected by the user, if any.
@property(nonatomic, assign) autofill::CreditCard* selectedPaymentMethod;

// The delegate to be notified when the user selects a payment method or returns
// without selecting a payment method.
@property(nonatomic, weak) id<PaymentMethodSelectionCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_COORDINATOR_H_
