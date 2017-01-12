// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_option_selection_coordinator.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/payments/shipping_option_selection_view_controller.h"

@interface ShippingOptionSelectionCoordinator ()<
    ShippingOptionSelectionViewControllerDelegate> {
  base::WeakNSProtocol<id<ShippingOptionSelectionCoordinatorDelegate>>
      _delegate;
  base::scoped_nsobject<ShippingOptionSelectionViewController> _viewController;
}

// Called when the user selects a shipping option. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection;

@end

@implementation ShippingOptionSelectionCoordinator

@synthesize shippingOptions = _shippingOptions;
@synthesize selectedShippingOption = _selectedShippingOption;

- (id<ShippingOptionSelectionCoordinatorDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<ShippingOptionSelectionCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)start {
  _viewController.reset([[ShippingOptionSelectionViewController alloc] init]);
  [_viewController setShippingOptions:_shippingOptions];
  [_viewController setSelectedShippingOption:_selectedShippingOption];
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

#pragma mark - ShippingOptionSelectionViewControllerDelegate

- (void)shippingOptionSelectionViewController:
            (ShippingOptionSelectionViewController*)controller
                       selectedShippingOption:
                           (web::PaymentShippingOption*)shippingOption {
  _selectedShippingOption = shippingOption;
  [self delayedNotifyDelegateOfSelection];
}

- (void)shippingOptionSelectionViewControllerDidReturn:
    (ShippingOptionSelectionViewController*)controller {
  [_delegate shippingOptionSelectionCoordinatorDidReturn:self];
}

- (void)delayedNotifyDelegateOfSelection {
  _viewController.get().view.userInteractionEnabled = NO;
  base::WeakNSObject<ShippingOptionSelectionCoordinator> weakSelf(self);
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(.2 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        base::scoped_nsobject<ShippingOptionSelectionCoordinator> strongSelf(
            [weakSelf retain]);
        // Early return if the coordinator has been deallocated.
        if (!strongSelf)
          return;

        _viewController.get().view.userInteractionEnabled = YES;
        [_delegate shippingOptionSelectionCoordinator:self
                               selectedShippingOption:_selectedShippingOption];
      });
}

@end
