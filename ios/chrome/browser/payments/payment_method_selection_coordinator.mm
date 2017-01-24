// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_coordinator.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "components/autofill/core/browser/credit_card.h"

@interface PaymentMethodSelectionCoordinator () {
  base::WeakNSProtocol<id<PaymentMethodSelectionCoordinatorDelegate>> _delegate;
  base::scoped_nsobject<PaymentMethodSelectionViewController> _viewController;
}

@end

@implementation PaymentMethodSelectionCoordinator

@synthesize paymentMethods = _paymentMethods;
@synthesize selectedPaymentMethod = _selectedPaymentMethod;

- (id<PaymentMethodSelectionCoordinatorDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<PaymentMethodSelectionCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)start {
  _viewController.reset([[PaymentMethodSelectionViewController alloc] init]);
  [_viewController setPaymentMethods:_paymentMethods];
  [_viewController setSelectedPaymentMethod:_selectedPaymentMethod];
  [_viewController setDelegate:self];
  [_viewController loadModel];

  DCHECK([self baseViewController].navigationController);
  [[self baseViewController].navigationController
      pushViewController:_viewController
                animated:YES];
}

- (void)stop {
  [[self baseViewController].navigationController
      popViewControllerAnimated:YES];
  _viewController.reset();
}

#pragma mark - PaymentMethodSelectionViewControllerDelegate

- (void)paymentMethodSelectionViewController:
            (PaymentMethodSelectionViewController*)controller
                      didSelectPaymentMethod:
                          (autofill::CreditCard*)paymentMethod {
  _selectedPaymentMethod = paymentMethod;
  [_delegate paymentMethodSelectionCoordinator:self
                        didSelectPaymentMethod:paymentMethod];
}

- (void)paymentMethodSelectionViewControllerDidReturn:
    (PaymentMethodSelectionViewController*)controller {
  [_delegate paymentMethodSelectionCoordinatorDidReturn:self];
}

@end
