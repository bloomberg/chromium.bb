// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_coordinator.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/ui/payments/payment_request_mediator.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Time interval before updating the Payment Summary item in seconds.
const NSTimeInterval kUpdatePaymentSummaryItemIntervalSeconds = 10.0;
}  // namespace

@implementation PaymentRequestCoordinator {
  UINavigationController* _navigationController;
  AddressEditCoordinator* _addressEditCoordinator;
  CreditCardEditCoordinator* _creditCardEditCoordinator;
  ContactInfoEditCoordinator* _contactInfoEditCoordinator;
  ContactInfoSelectionCoordinator* _contactInfoSelectionCoordinator;
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

  // Timer used to update the Payment Summary item.
  NSTimer* _updatePaymentSummaryItemTimer;
}

@synthesize paymentRequest = _paymentRequest;
@synthesize autofillManager = _autofillManager;
@synthesize browserState = _browserState;
@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;
@synthesize connectionSecure = _connectionSecure;
@synthesize delegate = _delegate;

- (void)start {
  _mediator =
      [[PaymentRequestMediator alloc] initWithPaymentRequest:_paymentRequest];

  _viewController = [[PaymentRequestViewController alloc] init];
  [_viewController setPageFavicon:_pageFavicon];
  [_viewController setPageTitle:_pageTitle];
  [_viewController setPageHost:_pageHost];
  [_viewController setConnectionSecure:_connectionSecure];
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

  _fullCardRequester = base::MakeUnique<FullCardRequester>(
      self, _navigationController, _browserState);

  [[self baseViewController] presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [_updatePaymentSummaryItemTimer invalidate];

  [[_navigationController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  [_addressEditCoordinator stop];
  _addressEditCoordinator = nil;
  [_creditCardEditCoordinator stop];
  _creditCardEditCoordinator = nil;
  [_contactInfoEditCoordinator stop];
  _contactInfoEditCoordinator = nil;
  [_itemsDisplayCoordinator stop];
  _itemsDisplayCoordinator = nil;
  [_contactInfoSelectionCoordinator stop];
  _contactInfoSelectionCoordinator = nil;
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

- (void)
requestFullCreditCard:(const autofill::CreditCard&)card
       resultDelegate:
           (base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>)
               resultDelegate {
  _fullCardRequester->GetFullCard(card, _autofillManager, resultDelegate);
}

#pragma mark - FullCardRequesterConsumer

- (void)fullCardRequestDidSucceedWithMethodName:(const std::string&)methodName
                             stringifiedDetails:
                                 (const std::string&)stringifiedDetails {
  _viewController.view.userInteractionEnabled = NO;
  [_viewController setPending:YES];
  [_viewController loadModel];
  [[_viewController collectionView] reloadData];

  [_delegate paymentRequestCoordinator:self
              didReceiveFullMethodName:methodName
                    stringifiedDetails:stringifiedDetails];
}

#pragma mark - Public methods

- (void)updatePaymentDetails:(web::PaymentDetails)paymentDetails {
  [_updatePaymentSummaryItemTimer invalidate];

  BOOL totalValueChanged =
      (_paymentRequest->payment_details().total != paymentDetails.total);
  [_mediator setTotalValueChanged:totalValueChanged];
  // Update the payment summary item.
  [_viewController updatePaymentSummaryItem];

  if (totalValueChanged) {
    // If the total value changed, update the Payment Summary item after a
    // certain time interval in order to clear the 'Updated' label on the item.
    _updatePaymentSummaryItemTimer = [NSTimer
        scheduledTimerWithTimeInterval:kUpdatePaymentSummaryItemIntervalSeconds
                                target:_viewController
                              selector:@selector(updatePaymentSummaryItem)
                              userInfo:nil
                               repeats:NO];
  }

  _paymentRequest->UpdatePaymentDetails(paymentDetails);

  // If a shipping address has been selected and there are available shipping
  // options, set it as the selected shipping address.
  if (_pendingShippingAddress && !_paymentRequest->shipping_options().empty()) {
    _paymentRequest->set_selected_shipping_profile(_pendingShippingAddress);
  }
  _pendingShippingAddress = nil;

  // Update the shipping section. The available shipping addresses/options and
  // the selected shipping address/option are already up-to-date.
  [_viewController updateShippingSection];

  if (_paymentRequest->shipping_options().empty()) {
    // Display error in the shipping address selection view, if present.
    [_shippingAddressSelectionCoordinator stopSpinnerAndDisplayError];

    // Display error in the shipping option selection view, if present.
    [_shippingOptionSelectionCoordinator stopSpinnerAndDisplayError];
  } else {
    // Dismiss the shipping address selection view, if present.
    [_shippingAddressSelectionCoordinator stop];
    _shippingAddressSelectionCoordinator = nil;

    // Dismiss the shipping option selection view, if present.
    [_shippingOptionSelectionCoordinator stop];
    _shippingOptionSelectionCoordinator = nil;
  }

  // Dismiss the address edit view, if present.
  [_addressEditCoordinator stop];
  _addressEditCoordinator = nil;
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
  DCHECK(_paymentRequest->selected_payment_method());

  [self recordPayButtonTapped];

  _paymentRequest->selected_payment_method()->InvokePaymentApp(
      _fullCardRequester.get());
}

- (void)paymentRequestViewControllerDidSelectSettings:
    (PaymentRequestViewController*)controller {
  [_delegate paymentRequestCoordinatorDidSelectSettings:self];
}

- (void)paymentRequestViewControllerDidSelectPaymentSummaryItem:
    (PaymentRequestViewController*)controller {
  // Return if there are no display items.
  if (_paymentRequest->payment_details().display_items.empty())
    return;

  _itemsDisplayCoordinator = [[PaymentItemsDisplayCoordinator alloc]
      initWithBaseViewController:_viewController];
  [_itemsDisplayCoordinator setPaymentRequest:_paymentRequest];
  [_itemsDisplayCoordinator setDelegate:self];

  [_itemsDisplayCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectContactInfoItem:
    (PaymentRequestViewController*)controller {
  if (_paymentRequest->contact_profiles().empty()) {
    _contactInfoEditCoordinator = [[ContactInfoEditCoordinator alloc]
        initWithBaseViewController:_viewController];
    [_contactInfoEditCoordinator setPaymentRequest:_paymentRequest];
    [_contactInfoEditCoordinator setDelegate:self];

    [_contactInfoEditCoordinator start];
    return;
  }

  _contactInfoSelectionCoordinator = [[ContactInfoSelectionCoordinator alloc]
      initWithBaseViewController:_viewController];
  [_contactInfoSelectionCoordinator setPaymentRequest:_paymentRequest];
  [_contactInfoSelectionCoordinator setDelegate:self];

  [_contactInfoSelectionCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectShippingAddressItem:
    (PaymentRequestViewController*)controller {
  if (_paymentRequest->shipping_profiles().empty()) {
    _addressEditCoordinator = [[AddressEditCoordinator alloc]
        initWithBaseViewController:_viewController];
    [_addressEditCoordinator setPaymentRequest:_paymentRequest];
    [_addressEditCoordinator setDelegate:self];

    [_addressEditCoordinator start];
    return;
  }

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
  if (_paymentRequest->payment_methods().empty()) {
    _creditCardEditCoordinator = [[CreditCardEditCoordinator alloc]
        initWithBaseViewController:_viewController];
    [_creditCardEditCoordinator setPaymentRequest:_paymentRequest];
    [_creditCardEditCoordinator setDelegate:self];

    [_creditCardEditCoordinator start];
    return;
  }

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
  [_itemsDisplayCoordinator stop];
  _itemsDisplayCoordinator = nil;
}

- (void)paymentItemsDisplayCoordinatorDidConfirm:
    (PaymentItemsDisplayCoordinator*)coordinator {
  DCHECK(_paymentRequest->selected_payment_method());

  [self recordPayButtonTapped];

  _paymentRequest->selected_payment_method()->InvokePaymentApp(
      _fullCardRequester.get());
}

#pragma mark - ContactInfoSelectionCoordinatorDelegate

- (void)
contactInfoSelectionCoordinator:(ContactInfoSelectionCoordinator*)coordinator
        didSelectContactProfile:(autofill::AutofillProfile*)contactProfile {
  DCHECK(contactProfile);
  _paymentRequest->set_selected_contact_profile(contactProfile);
  [_viewController updateContactInfoSection];

  [_contactInfoSelectionCoordinator stop];
  _contactInfoSelectionCoordinator = nil;
}

- (void)contactInfoSelectionCoordinatorDidReturn:
    (ContactInfoSelectionCoordinator*)coordinator {
  [_contactInfoSelectionCoordinator stop];
  _contactInfoSelectionCoordinator = nil;
}

#pragma mark - ContactInfoEditCoordinatorDelegate

- (void)contactInfoEditCoordinator:(ContactInfoEditCoordinator*)coordinator
           didFinishEditingProfile:(autofill::AutofillProfile*)profile {
  DCHECK(profile);
  _paymentRequest->set_selected_contact_profile(profile);
  [_viewController updateContactInfoSection];

  [_contactInfoEditCoordinator stop];
  _contactInfoEditCoordinator = nil;
}

- (void)contactInfoEditCoordinatorDidCancel:
    (ContactInfoEditCoordinator*)coordinator {
  [_contactInfoEditCoordinator stop];
  _contactInfoEditCoordinator = nil;
}

#pragma mark - ShippingAddressSelectionCoordinatorDelegate

- (void)shippingAddressSelectionCoordinator:
            (ShippingAddressSelectionCoordinator*)coordinator
                   didSelectShippingAddress:
                       (autofill::AutofillProfile*)shippingAddress {
  _pendingShippingAddress = shippingAddress;
  DCHECK(shippingAddress);
  [_delegate paymentRequestCoordinator:self
              didSelectShippingAddress:*shippingAddress];
}

- (void)shippingAddressSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {

  [_shippingAddressSelectionCoordinator stop];
  _shippingAddressSelectionCoordinator = nil;
}

#pragma mark - AddressEditCoordinatorDelegate

- (void)addressEditCoordinator:(AddressEditCoordinator*)coordinator
       didFinishEditingAddress:(autofill::AutofillProfile*)address {
  _pendingShippingAddress = address;
  DCHECK(address);
  [_delegate paymentRequestCoordinator:self didSelectShippingAddress:*address];
}

- (void)addressEditCoordinatorDidCancel:(AddressEditCoordinator*)coordinator {
  [_addressEditCoordinator stop];
  _addressEditCoordinator = nil;
}

#pragma mark - ShippingOptionSelectionCoordinatorDelegate

- (void)shippingOptionSelectionCoordinator:
            (ShippingOptionSelectionCoordinator*)coordinator
                   didSelectShippingOption:
                       (web::PaymentShippingOption*)shippingOption {
  DCHECK(shippingOption);
  [_delegate paymentRequestCoordinator:self
               didSelectShippingOption:*shippingOption];
}

- (void)shippingOptionSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {
  [_shippingOptionSelectionCoordinator stop];
  _shippingOptionSelectionCoordinator = nil;
}

#pragma mark - PaymentMethodSelectionCoordinatorDelegate

- (void)paymentMethodSelectionCoordinator:
            (PaymentMethodSelectionCoordinator*)coordinator
                   didSelectPaymentMethod:
                       (payments::PaymentInstrument*)paymentMethod {
  DCHECK(paymentMethod);
  DCHECK(paymentMethod->IsCompleteForPayment());
  _paymentRequest->set_selected_payment_method(paymentMethod);
  [_viewController updatePaymentMethodSection];

  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator = nil;
}

- (void)paymentMethodSelectionCoordinatorDidReturn:
    (PaymentMethodSelectionCoordinator*)coordinator {
  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator = nil;
}

#pragma mark - CreditCardEditCoordinatorDelegate

- (void)creditCardEditCoordinator:(CreditCardEditCoordinator*)coordinator
    didFinishEditingPaymentMethod:
        (payments::AutofillPaymentInstrument*)paymentMethod {
  DCHECK(paymentMethod);
  DCHECK(paymentMethod->IsCompleteForPayment());
  _paymentRequest->set_selected_payment_method(paymentMethod);
  [_viewController updatePaymentMethodSection];

  [_creditCardEditCoordinator stop];
  _creditCardEditCoordinator = nil;
}

- (void)creditCardEditCoordinatorDidCancel:
    (CreditCardEditCoordinator*)coordinator {
  [_creditCardEditCoordinator stop];
  _creditCardEditCoordinator = nil;
}

#pragma mark - Helper methods

- (void)recordPayButtonTapped {
  _paymentRequest->journey_logger().SetEventOccurred(
      payments::JourneyLogger::EVENT_PAY_CLICKED);
  _paymentRequest->journey_logger().SetSelectedPaymentMethod(
      _paymentRequest->selected_payment_method()->type() ==
              payments::PaymentInstrument::Type::AUTOFILL
          ? payments::JourneyLogger::SELECTED_PAYMENT_METHOD_CREDIT_CARD
          : payments::JourneyLogger::SELECTED_PAYMENT_METHOD_OTHER_PAYMENT_APP);
}

@end
