// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_option_selection_coordinator.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/payments/payment_request_util.h"

@interface ShippingOptionSelectionCoordinator () {
  base::WeakNSProtocol<id<ShippingOptionSelectionCoordinatorDelegate>>
      _delegate;
  base::scoped_nsobject<ShippingOptionSelectionViewController> _viewController;
}

// Called when the user selects a shipping option. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection:
    (web::PaymentShippingOption*)shippingOption;

@end

@implementation ShippingOptionSelectionCoordinator

@synthesize paymentRequest = _paymentRequest;

- (id<ShippingOptionSelectionCoordinatorDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<ShippingOptionSelectionCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)start {
  _viewController.reset([[ShippingOptionSelectionViewController alloc]
      initWithPaymentRequest:_paymentRequest]);
  [_viewController setDelegate:self];
  [_viewController loadModel];

  DCHECK(self.baseViewController.navigationController);
  [self.baseViewController.navigationController
      pushViewController:_viewController
                animated:YES];
}

- (void)stop {
  [self.baseViewController.navigationController popViewControllerAnimated:YES];
  _viewController.reset();
}

- (void)stopSpinnerAndDisplayError {
  // Re-enable user interactions that were disabled earlier in
  // delayedNotifyDelegateOfSelection.
  _viewController.get().view.userInteractionEnabled = YES;

  [_viewController setIsLoading:NO];
  NSString* errorMessage =
      payment_request_util::GetShippingOptionSelectorErrorMessage(
          _paymentRequest);
  [_viewController setErrorMessage:errorMessage];
  [_viewController loadModel];
  [[_viewController collectionView] reloadData];
}

#pragma mark - ShippingOptionSelectionViewControllerDelegate

- (void)shippingOptionSelectionViewController:
            (ShippingOptionSelectionViewController*)controller
                      didSelectShippingOption:
                          (web::PaymentShippingOption*)shippingOption {
  [self delayedNotifyDelegateOfSelection:shippingOption];
}

- (void)shippingOptionSelectionViewControllerDidReturn:
    (ShippingOptionSelectionViewController*)controller {
  [_delegate shippingOptionSelectionCoordinatorDidReturn:self];
}

- (void)delayedNotifyDelegateOfSelection:
    (web::PaymentShippingOption*)shippingOption {
  _viewController.get().view.userInteractionEnabled = NO;
  base::WeakNSObject<ShippingOptionSelectionCoordinator> weakSelf(self);
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    static_cast<int64_t>(0.2 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        base::scoped_nsobject<ShippingOptionSelectionCoordinator> strongSelf(
            [weakSelf retain]);
        // Early return if the coordinator has been deallocated.
        if (!strongSelf)
          return;
        [_viewController setIsLoading:YES];
        [_viewController loadModel];
        [[_viewController collectionView] reloadData];

        [_delegate shippingOptionSelectionCoordinator:self
                              didSelectShippingOption:shippingOption];
      });
}

@end
