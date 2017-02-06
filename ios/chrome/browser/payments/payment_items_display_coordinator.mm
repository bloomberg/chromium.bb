// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_items_display_coordinator.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/web/public/payments/payment_request.h"

@interface PaymentItemsDisplayCoordinator ()<
    PaymentItemsDisplayViewControllerDelegate> {
  base::WeakNSProtocol<id<PaymentItemsDisplayCoordinatorDelegate>> _delegate;
  base::scoped_nsobject<PaymentItemsDisplayViewController> _viewController;
}

@end

@implementation PaymentItemsDisplayCoordinator

@synthesize paymentRequest = _paymentRequest;

- (id<PaymentItemsDisplayCoordinatorDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<PaymentItemsDisplayCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)start {
  BOOL payButtonEnabled = _paymentRequest->selected_credit_card() != nil;
  _viewController.reset([[PaymentItemsDisplayViewController alloc]
      initWithPaymentRequest:_paymentRequest
            payButtonEnabled:payButtonEnabled]);
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

#pragma mark - PaymentItemsDisplayViewControllerDelegate

- (void)paymentItemsDisplayViewControllerDidReturn:
    (PaymentItemsDisplayViewController*)controller {
  [_delegate paymentItemsDisplayCoordinatorDidReturn:self];
}

- (void)paymentItemsDisplayViewControllerDidConfirm:
    (PaymentItemsDisplayViewController*)controller {
  [_delegate paymentItemsDisplayCoordinatorDidConfirm:self];
}

@end
