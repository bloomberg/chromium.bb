// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_address_selection_coordinator.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "components/autofill/core/browser/autofill_profile.h"
#import "ios/chrome/browser/payments/shipping_address_selection_view_controller.h"

@interface ShippingAddressSelectionCoordinator ()<
    ShippingAddressSelectionViewControllerDelegate> {
  base::WeakNSProtocol<id<ShippingAddressSelectionCoordinatorDelegate>>
      _delegate;
  base::scoped_nsobject<ShippingAddressSelectionViewController> _viewController;
}

// Called when the user selects a shipping address. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection;

@end

@implementation ShippingAddressSelectionCoordinator

@synthesize shippingAddresses = _shippingAddresses;
@synthesize selectedShippingAddress = _selectedShippingAddress;

- (id<ShippingAddressSelectionCoordinatorDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<ShippingAddressSelectionCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)start {
  _viewController.reset([[ShippingAddressSelectionViewController alloc] init]);
  [_viewController setShippingAddresses:_shippingAddresses];
  [_viewController setSelectedShippingAddress:_selectedShippingAddress];
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

#pragma mark - ShippingAddressSelectionViewControllerDelegate

- (void)shippingAddressSelectionViewController:
            (ShippingAddressSelectionViewController*)controller
                       selectedShippingAddress:
                           (autofill::AutofillProfile*)shippingAddress {
  _selectedShippingAddress = shippingAddress;
  [self delayedNotifyDelegateOfSelection];
}

- (void)shippingAddressSelectionViewControllerDidReturn:
    (ShippingAddressSelectionViewController*)controller {
  [_delegate shippingAddressSelectionCoordinatorDidReturn:self];
}

- (void)delayedNotifyDelegateOfSelection {
  _viewController.get().view.userInteractionEnabled = NO;
  base::WeakNSObject<ShippingAddressSelectionCoordinator> weakSelf(self);
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(.2 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        base::scoped_nsobject<ShippingAddressSelectionCoordinator> strongSelf(
            [weakSelf retain]);
        // Early return if the coordinator has been deallocated.
        if (!strongSelf)
          return;

        _viewController.get().view.userInteractionEnabled = YES;
        [_delegate
            shippingAddressSelectionCoordinator:self
                        selectedShippingAddress:_selectedShippingAddress];
      });
}

@end
