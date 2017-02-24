// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_items_display_coordinator.h"

#include "base/logging.h"
#include "ios/web/public/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentItemsDisplayCoordinator () {
  PaymentItemsDisplayViewController* _viewController;
}

@end

@implementation PaymentItemsDisplayCoordinator

@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;

- (void)start {
  BOOL payButtonEnabled = _paymentRequest->selected_credit_card() != nil;
  _viewController = [[PaymentItemsDisplayViewController alloc]
      initWithPaymentRequest:_paymentRequest
            payButtonEnabled:payButtonEnabled];
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
  _viewController = nil;
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
