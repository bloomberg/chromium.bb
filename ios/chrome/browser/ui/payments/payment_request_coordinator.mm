// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_coordinator.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/ui/payments/full_card_requester.h"
#include "ios/chrome/browser/ui/payments/payment_request_mediator.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payments::data_util::GetPaymentAddressFromAutofillProfile;
}  // namespace

@implementation PaymentRequestCoordinator {
  UINavigationController* _navigationController;
  PaymentRequestViewController* _viewController;
  PaymentRequestErrorCoordinator* _errorCoordinator;
  PaymentItemsDisplayCoordinator* _itemsDisplayCoordinator;
  ShippingAddressSelectionCoordinator* _shippingAddressSelectionCoordinator;
  ShippingOptionSelectionCoordinator* _shippingOptionSelectionCoordinator;
  PaymentMethodSelectionCoordinator* _methodSelectionCoordinator;

  PaymentRequestMediator* _mediator;

  // Receiver of the full credit card details. Also displays the unmask prompt
  // UI.
  std::unique_ptr<FullCardRequester> _fullCardRequester;

  // The selected shipping address, pending approval from the page.
  autofill::AutofillProfile* _pendingShippingAddress;
}

@synthesize paymentRequest = _paymentRequest;
@synthesize autofillManager = _autofillManager;
@synthesize browserState = _browserState;
@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;
@synthesize delegate = _delegate;

- (void)start {
  _mediator =
      [[PaymentRequestMediator alloc] initWithBrowserState:_browserState];

  _viewController = [[PaymentRequestViewController alloc]
      initWithPaymentRequest:_paymentRequest];
  [_viewController setPageFavicon:_pageFavicon];
  [_viewController setPageTitle:_pageTitle];
  [_viewController setPageHost:_pageHost];
  [_viewController setDelegate:self];
  [_viewController setDataSource:_mediator];
  [_viewController loadModel];

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [_navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  [_navigationController
      setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  [_navigationController setNavigationBarHidden:YES];

  [[self baseViewController] presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [[_navigationController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  [_itemsDisplayCoordinator stop];
  _itemsDisplayCoordinator = nil;
  [_shippingAddressSelectionCoordinator stop];
  _shippingAddressSelectionCoordinator = nil;
  [_shippingOptionSelectionCoordinator stop];
  _shippingOptionSelectionCoordinator = nil;
  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator = nil;
  [_errorCoordinator stop];
  _errorCoordinator = nil;
  _viewController = nil;
  _navigationController = nil;
}

- (void)sendPaymentResponse {
  DCHECK(_paymentRequest->selected_credit_card());
  autofill::CreditCard* card = _paymentRequest->selected_credit_card();
  _fullCardRequester = base::MakeUnique<FullCardRequester>(
      self, _navigationController, _browserState);
  _fullCardRequester->GetFullCard(card, _autofillManager);
}

- (void)fullCardRequestDidSucceedWithCard:(const autofill::CreditCard&)card
                         verificationCode:
                             (const base::string16&)verificationCode {
  _viewController.view.userInteractionEnabled = NO;
  [_viewController setPending:YES];
  [_viewController loadModel];
  [[_viewController collectionView] reloadData];

  [_delegate paymentRequestCoordinator:self
             didCompletePaymentRequest:_paymentRequest
                                  card:card
                      verificationCode:verificationCode];
}

- (void)updatePaymentDetails:(web::PaymentDetails)paymentDetails {
  BOOL totalValueChanged =
      (_paymentRequest->payment_details().total != paymentDetails.total);
  _paymentRequest->UpdatePaymentDetails(paymentDetails);

  if (_paymentRequest->shipping_options().empty()) {
    // Display error in the shipping address/option selection view.
    if (_shippingAddressSelectionCoordinator) {
      _paymentRequest->set_selected_shipping_profile(nil);
      [_shippingAddressSelectionCoordinator stopSpinnerAndDisplayError];
    } else if (_shippingOptionSelectionCoordinator) {
      [_shippingOptionSelectionCoordinator stopSpinnerAndDisplayError];
    }
    // Update the payment request summary view.
    [_viewController loadModel];
    [[_viewController collectionView] reloadData];
  } else {
    // Update the payment summary section.
    [_viewController
        updatePaymentSummaryWithTotalValueChanged:totalValueChanged];

    if (_shippingAddressSelectionCoordinator) {
      // Set the selected shipping address and update the selected shipping
      // address in the payment request summary view.
      _paymentRequest->set_selected_shipping_profile(_pendingShippingAddress);
      _pendingShippingAddress = nil;
      [_viewController updateSelectedShippingAddressUI];

      // Dismiss the shipping address selection view.
      [_shippingAddressSelectionCoordinator stop];
      _shippingAddressSelectionCoordinator = nil;
    } else if (_shippingOptionSelectionCoordinator) {
      // Update the selected shipping option in the payment request summary
      // view. The updated selection is already reflected in |_paymentRequest|.
      [_viewController updateSelectedShippingOptionUI];

      // Dismiss the shipping option selection view.
      [_shippingOptionSelectionCoordinator stop];
      _shippingOptionSelectionCoordinator = nil;
    }
  }
}

- (void)displayErrorWithCallback:(ProceduralBlock)callback {
  _errorCoordinator = [[PaymentRequestErrorCoordinator alloc]
      initWithBaseViewController:_navigationController];
  [_errorCoordinator setCallback:callback];
  [_errorCoordinator setDelegate:self];

  [_errorCoordinator start];
}

#pragma mark - PaymentRequestViewControllerDelegate

- (void)paymentRequestViewControllerDidCancel:
    (PaymentRequestViewController*)controller {
  [_delegate paymentRequestCoordinatorDidCancel:self];
}

- (void)paymentRequestViewControllerDidConfirm:
    (PaymentRequestViewController*)controller {
  [self sendPaymentResponse];
}

- (void)paymentRequestViewControllerDidSelectSettings:
    (PaymentRequestViewController*)controller {
  [_delegate paymentRequestCoordinatorDidSelectSettings:self];
}

- (void)paymentRequestViewControllerDidSelectPaymentSummaryItem:
    (PaymentRequestViewController*)controller {
  _itemsDisplayCoordinator = [[PaymentItemsDisplayCoordinator alloc]
      initWithBaseViewController:_viewController];
  [_itemsDisplayCoordinator setPaymentRequest:_paymentRequest];
  [_itemsDisplayCoordinator setDelegate:self];

  [_itemsDisplayCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectShippingAddressItem:
    (PaymentRequestViewController*)controller {
  _shippingAddressSelectionCoordinator =
      [[ShippingAddressSelectionCoordinator alloc]
          initWithBaseViewController:_viewController];
  [_shippingAddressSelectionCoordinator setPaymentRequest:_paymentRequest];
  [_shippingAddressSelectionCoordinator setDelegate:self];

  [_shippingAddressSelectionCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectShippingOptionItem:
    (PaymentRequestViewController*)controller {
  _shippingOptionSelectionCoordinator =
      [[ShippingOptionSelectionCoordinator alloc]
          initWithBaseViewController:_viewController];
  [_shippingOptionSelectionCoordinator setPaymentRequest:_paymentRequest];
  [_shippingOptionSelectionCoordinator setDelegate:self];

  [_shippingOptionSelectionCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectPaymentMethodItem:
    (PaymentRequestViewController*)controller {
  _methodSelectionCoordinator = [[PaymentMethodSelectionCoordinator alloc]
      initWithBaseViewController:_viewController];
  [_methodSelectionCoordinator setPaymentRequest:_paymentRequest];
  [_methodSelectionCoordinator setDelegate:self];

  [_methodSelectionCoordinator start];
}

#pragma mark - PaymentRequestErrorCoordinatorDelegate

- (void)paymentRequestErrorCoordinatorDidDismiss:
    (PaymentRequestErrorCoordinator*)coordinator {
  ProceduralBlock callback = coordinator.callback;

  [_errorCoordinator stop];
  _errorCoordinator = nil;

  if (callback)
    callback();
}

#pragma mark - PaymentItemsDisplayCoordinatorDelegate

- (void)paymentItemsDisplayCoordinatorDidReturn:
    (PaymentItemsDisplayCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_itemsDisplayCoordinator stop];
  _itemsDisplayCoordinator = nil;
}

- (void)paymentItemsDisplayCoordinatorDidConfirm:
    (PaymentItemsDisplayCoordinator*)coordinator {
  [self sendPaymentResponse];
}

#pragma mark - ShippingAddressSelectionCoordinatorDelegate

- (void)shippingAddressSelectionCoordinator:
            (ShippingAddressSelectionCoordinator*)coordinator
                   didSelectShippingAddress:
                       (autofill::AutofillProfile*)shippingAddress {
  _pendingShippingAddress = shippingAddress;
  DCHECK(shippingAddress);
  payments::PaymentAddress address = GetPaymentAddressFromAutofillProfile(
      *shippingAddress, GetApplicationContext()->GetApplicationLocale());
  [_delegate paymentRequestCoordinator:self didSelectShippingAddress:address];
}

- (void)shippingAddressSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_shippingAddressSelectionCoordinator stop];
  _shippingAddressSelectionCoordinator = nil;
}

#pragma mark - ShippingOptionSelectionCoordinatorDelegate

- (void)shippingOptionSelectionCoordinator:
            (ShippingOptionSelectionCoordinator*)coordinator
                   didSelectShippingOption:
                       (web::PaymentShippingOption*)shippingOption {
  [_delegate paymentRequestCoordinator:self
               didSelectShippingOption:*shippingOption];
}

- (void)shippingOptionSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_shippingOptionSelectionCoordinator stop];
  _shippingOptionSelectionCoordinator = nil;
}

#pragma mark - PaymentMethodSelectionCoordinatorDelegate

- (void)paymentMethodSelectionCoordinator:
            (PaymentMethodSelectionCoordinator*)coordinator
                   didSelectPaymentMethod:(autofill::CreditCard*)creditCard {
  _paymentRequest->set_selected_credit_card(creditCard);

  [_viewController updateSelectedPaymentMethodUI];

  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator = nil;
}

- (void)paymentMethodSelectionCoordinatorDidReturn:
    (PaymentMethodSelectionCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator = nil;
}

@end
