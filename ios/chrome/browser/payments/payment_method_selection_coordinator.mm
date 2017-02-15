// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_coordinator.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "components/autofill/core/browser/credit_card.h"
#include "ios/chrome/browser/payments/payment_request.h"

@interface PaymentMethodSelectionCoordinator () {
  base::WeakNSProtocol<id<PaymentMethodSelectionCoordinatorDelegate>> _delegate;
  base::scoped_nsobject<PaymentMethodSelectionViewController> _viewController;
}

// Called when the user selects a payment method. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection:(autofill::CreditCard*)paymentMethod;

@end

@implementation PaymentMethodSelectionCoordinator

@synthesize paymentRequest = _paymentRequest;

- (id<PaymentMethodSelectionCoordinatorDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<PaymentMethodSelectionCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)start {
  _viewController.reset([[PaymentMethodSelectionViewController alloc]
      initWithPaymentRequest:_paymentRequest]);
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
  [self delayedNotifyDelegateOfSelection:paymentMethod];
}

- (void)paymentMethodSelectionViewControllerDidReturn:
    (PaymentMethodSelectionViewController*)controller {
  [_delegate paymentMethodSelectionCoordinatorDidReturn:self];
}

- (void)delayedNotifyDelegateOfSelection:(autofill::CreditCard*)paymentMethod {
  _viewController.get().view.userInteractionEnabled = NO;
  base::WeakNSObject<PaymentMethodSelectionCoordinator> weakSelf(self);
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    static_cast<int64_t>(0.2 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        base::scoped_nsobject<PaymentMethodSelectionCoordinator> strongSelf(
            [weakSelf retain]);
        // Early return if the coordinator has been deallocated.
        if (!strongSelf)
          return;

        _viewController.get().view.userInteractionEnabled = YES;
        [_delegate paymentMethodSelectionCoordinator:self
                              didSelectPaymentMethod:paymentMethod];
      });
}

@end
