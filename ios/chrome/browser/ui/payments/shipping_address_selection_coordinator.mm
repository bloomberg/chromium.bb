// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/shipping_address_selection_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/payments/core/strings_util.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"
#include "ios/chrome/browser/ui/payments/shipping_address_selection_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetShippingAddressSelectorErrorMessage;
using ::payments::GetShippingAddressSectionString;
using ::payments::GetShippingAddressSelectorInfoMessage;

// The delay in nano seconds before notifying the delegate of the selection.
const int64_t kDelegateNotificationDelayInNanoSeconds = 0.2 * NSEC_PER_SEC;
}  // namespace

@interface ShippingAddressSelectionCoordinator ()

@property(nonatomic, strong) AddressEditCoordinator* addressEditCoordinator;

@property(nonatomic, strong)
    PaymentRequestSelectorViewController* viewController;

@property(nonatomic, strong) ShippingAddressSelectionMediator* mediator;

// Called when the user selects a shipping address. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)shippingAddress;

@end

@implementation ShippingAddressSelectionCoordinator

@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize addressEditCoordinator = _addressEditCoordinator;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.mediator = [[ShippingAddressSelectionMediator alloc]
      initWithPaymentRequest:self.paymentRequest];
  self.mediator.headerText =
      self.paymentRequest->shipping_options().empty()
          ? base::SysUTF16ToNSString(GetShippingAddressSelectorInfoMessage(
                self.paymentRequest->shipping_type()))
          : nil;

  self.viewController = [[PaymentRequestSelectorViewController alloc] init];
  self.viewController.title = base::SysUTF16ToNSString(
      GetShippingAddressSectionString(self.paymentRequest->shipping_type()));
  self.viewController.delegate = self;
  self.viewController.dataSource = self.mediator;
  [self.viewController loadModel];

  DCHECK(self.baseViewController.navigationController);
  [self.baseViewController.navigationController
      pushViewController:self.viewController
                animated:YES];
}

- (void)stop {
  [self.baseViewController.navigationController popViewControllerAnimated:YES];
  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;
  self.viewController = nil;
  self.mediator = nil;
}

- (void)stopSpinnerAndDisplayError {
  // Re-enable user interactions that were disabled earlier in
  // delayedNotifyDelegateOfSelection.
  self.viewController.view.userInteractionEnabled = YES;

  DCHECK(self.paymentRequest);
  self.mediator.headerText =
      GetShippingAddressSelectorErrorMessage(*self.paymentRequest);
  self.mediator.state = PaymentRequestSelectorStateError;
  [self.viewController loadModel];
  [self.viewController.collectionView reloadData];
}

#pragma mark - PaymentRequestSelectorViewControllerDelegate

- (void)paymentRequestSelectorViewController:
            (PaymentRequestSelectorViewController*)controller
                        didSelectItemAtIndex:(NSUInteger)index {
  // Update the data source with the selection.
  self.mediator.selectedItemIndex = index;

  DCHECK(index < self.paymentRequest->shipping_profiles().size());
  [self delayedNotifyDelegateOfSelection:self.paymentRequest
                                             ->shipping_profiles()[index]];
}

- (void)paymentRequestSelectorViewControllerDidFinish:
    (PaymentRequestSelectorViewController*)controller {
  [self.delegate shippingAddressSelectionCoordinatorDidReturn:self];
}

- (void)paymentRequestSelectorViewControllerDidSelectAddItem:
    (PaymentRequestSelectorViewController*)controller {
  self.addressEditCoordinator = [[AddressEditCoordinator alloc]
      initWithBaseViewController:self.viewController];
  self.addressEditCoordinator.paymentRequest = self.paymentRequest;
  self.addressEditCoordinator.delegate = self;
  [self.addressEditCoordinator start];
}

#pragma mark - AddressEditCoordinatorDelegate

- (void)addressEditCoordinator:(AddressEditCoordinator*)coordinator
       didFinishEditingAddress:(autofill::AutofillProfile*)address {
  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;

  // Inform |self.delegate| that |address| has been selected.
  [self.delegate shippingAddressSelectionCoordinator:self
                            didSelectShippingAddress:address];
}

- (void)addressEditCoordinatorDidCancel:(AddressEditCoordinator*)coordinator {
  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;
}

#pragma mark - Helper methods

- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)shippingAddress {
  self.viewController.view.userInteractionEnabled = NO;
  __weak ShippingAddressSelectionCoordinator* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kDelegateNotificationDelayInNanoSeconds),
      dispatch_get_main_queue(), ^{
        [weakSelf.mediator setState:PaymentRequestSelectorStatePending];
        [weakSelf.viewController loadModel];
        [weakSelf.viewController.collectionView reloadData];

        [weakSelf.delegate shippingAddressSelectionCoordinator:weakSelf
                                      didSelectShippingAddress:shippingAddress];
      });
}

@end
