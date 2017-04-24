// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_method_selection_coordinator.h"

#include "components/autofill/core/browser/credit_card.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/ui/payments/payment_method_selection_mediator.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The delay in nano seconds before notifying the delegate of the selection.
const int64_t kDelegateNotificationDelayInNanoSeconds = 0.2 * NSEC_PER_SEC;
}  // namespace

@interface PaymentMethodSelectionCoordinator ()

@property(nonatomic, strong)
    CreditCardEditCoordinator* creditCardEditCoordinator;

@property(nonatomic, strong)
    PaymentRequestSelectorViewController* viewController;

@property(nonatomic, strong) PaymentMethodSelectionMediator* mediator;

// Called when the user selects a payment method. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified. The delay is here to let the user get a visual feedback of the
// selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection:(autofill::CreditCard*)paymentMethod;

@end

@implementation PaymentMethodSelectionCoordinator
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize creditCardEditCoordinator = _creditCardEditCoordinator;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.mediator = [[PaymentMethodSelectionMediator alloc]
      initWithPaymentRequest:self.paymentRequest];

  self.viewController = [[PaymentRequestSelectorViewController alloc] init];
  self.viewController.title =
      l10n_util::GetNSString(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL);
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
  [self.creditCardEditCoordinator stop];
  self.creditCardEditCoordinator = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - PaymentRequestSelectorViewControllerDelegate

- (void)paymentRequestSelectorViewController:
            (PaymentRequestSelectorViewController*)controller
                        didSelectItemAtIndex:(NSUInteger)index {
  // Update the data source with the selection.
  self.mediator.selectedItemIndex = index;

  DCHECK(index < self.paymentRequest->credit_cards().size());
  [self delayedNotifyDelegateOfSelection:self.paymentRequest
                                             ->credit_cards()[index]];
}

- (void)paymentRequestSelectorViewControllerDidFinish:
    (PaymentRequestSelectorViewController*)controller {
  [self.delegate paymentMethodSelectionCoordinatorDidReturn:self];
}

- (void)paymentRequestSelectorViewControllerDidSelectAddItem:
    (PaymentRequestSelectorViewController*)controller {
  self.creditCardEditCoordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:self.viewController];
  self.creditCardEditCoordinator.paymentRequest = self.paymentRequest;
  self.creditCardEditCoordinator.delegate = self;
  [self.creditCardEditCoordinator start];
}

#pragma mark - CreditCardEditCoordinatorDelegate

- (void)creditCardEditCoordinator:(CreditCardEditCoordinator*)coordinator
       didFinishEditingCreditCard:(autofill::CreditCard*)creditCard {
  [self.creditCardEditCoordinator stop];
  self.creditCardEditCoordinator = nil;

  // Inform |self.delegate| that this card has been selected.
  [self.delegate paymentMethodSelectionCoordinator:self
                            didSelectPaymentMethod:creditCard];
}

- (void)creditCardEditCoordinatorDidCancel:
    (CreditCardEditCoordinator*)coordinator {
  [self.creditCardEditCoordinator stop];
  self.creditCardEditCoordinator = nil;
}

#pragma mark - Helper methods

- (void)delayedNotifyDelegateOfSelection:(autofill::CreditCard*)paymentMethod {
  self.viewController.view.userInteractionEnabled = NO;
  __weak PaymentMethodSelectionCoordinator* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kDelegateNotificationDelayInNanoSeconds),
      dispatch_get_main_queue(), ^{
        weakSelf.viewController.view.userInteractionEnabled = YES;
        [weakSelf.delegate paymentMethodSelectionCoordinator:weakSelf
                                      didSelectPaymentMethod:paymentMethod];
      });
}

@end
