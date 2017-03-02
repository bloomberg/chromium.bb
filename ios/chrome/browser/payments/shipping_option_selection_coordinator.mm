// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_option_selection_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/payments/payment_request_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetShippingOptionSelectorErrorMessage;
}  // namespace

@interface ShippingOptionSelectionCoordinator ()

@property(nonatomic, strong)
    ShippingOptionSelectionViewController* viewController;

// Called when the user selects a shipping option. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection:
    (web::PaymentShippingOption*)shippingOption;

@end

@implementation ShippingOptionSelectionCoordinator

@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize viewController = _viewController;

- (void)start {
  _viewController = [[ShippingOptionSelectionViewController alloc]
      initWithPaymentRequest:_paymentRequest];
  [_viewController setDelegate:self];
  [_viewController loadModel];

  DCHECK(self.baseViewController.navigationController);
  [self.baseViewController.navigationController
      pushViewController:_viewController
                animated:YES];
}

- (void)stop {
  [self.baseViewController.navigationController popViewControllerAnimated:YES];
  _viewController = nil;
}

- (void)stopSpinnerAndDisplayError {
  // Re-enable user interactions that were disabled earlier in
  // delayedNotifyDelegateOfSelection.
  _viewController.view.userInteractionEnabled = YES;

  [_viewController setPending:NO];
  DCHECK(_paymentRequest);
  [_viewController
      setErrorMessage:GetShippingOptionSelectorErrorMessage(*_paymentRequest)];
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
  _viewController.view.userInteractionEnabled = NO;
  __weak ShippingOptionSelectionCoordinator* weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               static_cast<int64_t>(0.2 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   ShippingOptionSelectionCoordinator* strongSelf = weakSelf;
                   // Early return if the coordinator has been deallocated.
                   if (!strongSelf)
                     return;
                   [strongSelf.viewController setPending:YES];
                   [strongSelf.viewController loadModel];
                   [[strongSelf.viewController collectionView] reloadData];

                   [strongSelf.delegate
                       shippingOptionSelectionCoordinator:strongSelf
                                  didSelectShippingOption:shippingOption];
                 });
}

@end
