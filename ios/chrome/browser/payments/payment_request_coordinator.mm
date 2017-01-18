// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_coordinator.h"

#include <unordered_set>
#include <vector>

#import "base/ios/weak_nsobject.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/application_context.h"

@interface PaymentRequestCoordinator () {
  autofill::PersonalDataManager* _personalDataManager;  // weak
  base::WeakNSProtocol<id<PaymentRequestCoordinatorDelegate>> _delegate;
  base::scoped_nsobject<UINavigationController> _navigationController;
  base::scoped_nsobject<PaymentRequestViewController> _viewController;
  base::scoped_nsobject<PaymentItemsDisplayCoordinator>
      _itemsDisplayCoordinator;
  base::scoped_nsobject<ShippingAddressSelectionCoordinator>
      _shippingAddressSelectionCoordinator;
  base::scoped_nsobject<ShippingOptionSelectionCoordinator>
      _shippingOptionSelectionCoordinator;
  base::scoped_nsobject<PaymentMethodSelectionCoordinator>
      _methodSelectionCoordinator;

  base::mac::ObjCPropertyReleaser _propertyReleaser_PaymentRequestCoordinator;
}

// Returns the credit cards available from |_personalDataManager| that match
// a supported type specified in |_paymentRequest|.
- (std::vector<autofill::CreditCard*>)supportedMethods;

@end

@implementation PaymentRequestCoordinator

@synthesize paymentRequest = _paymentRequest;
@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;
@synthesize selectedShippingAddress = _selectedShippingAddress;
@synthesize selectedShippingOption = _selectedShippingOption;

@synthesize selectedPaymentMethod = _selectedPaymentMethod;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                       personalDataManager:
                           (autofill::PersonalDataManager*)personalDataManager {
  if ((self = [super initWithBaseViewController:baseViewController])) {
    _propertyReleaser_PaymentRequestCoordinator.Init(
        self, [PaymentRequestCoordinator class]);
    _personalDataManager = personalDataManager;
  }
  return self;
}

- (id<PaymentRequestCoordinatorDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<PaymentRequestCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)start {
  const std::vector<autofill::AutofillProfile*> addresses =
      _personalDataManager->GetProfilesToSuggest();
  if (addresses.size() > 0)
    _selectedShippingAddress = addresses[0];

  for (size_t i = 0; i < _paymentRequest.details.shipping_options.size(); ++i) {
    web::PaymentShippingOption* shippingOption =
        &_paymentRequest.details.shipping_options[i];
    if (shippingOption->selected) {
      // If more than one option has |selected| set, the last one in the
      // sequence should be treated as the selected item.
      _selectedShippingOption = shippingOption;
    }
  }

  const std::vector<autofill::CreditCard*> cards = [self supportedMethods];
  if (cards.size() > 0)
    _selectedPaymentMethod = cards[0];

  _viewController.reset([[PaymentRequestViewController alloc] init]);
  [_viewController setPaymentRequest:_paymentRequest];
  [_viewController setPageFavicon:_pageFavicon];
  [_viewController setPageTitle:_pageTitle];
  [_viewController setPageHost:_pageHost];
  [_viewController setSelectedShippingAddress:_selectedShippingAddress];
  [_viewController setSelectedShippingOption:_selectedShippingOption];
  [_viewController setSelectedPaymentMethod:_selectedPaymentMethod];
  [_viewController setDelegate:self];
  [_viewController loadModel];

  _navigationController.reset([[UINavigationController alloc]
      initWithRootViewController:_viewController]);
  [_navigationController setNavigationBarHidden:YES];

  [[self baseViewController] presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  _itemsDisplayCoordinator.reset();
  _shippingAddressSelectionCoordinator.reset();
  _shippingOptionSelectionCoordinator.reset();
  _methodSelectionCoordinator.reset();
  _navigationController.reset();
  _viewController.reset();
}

- (std::vector<autofill::CreditCard*>)supportedMethods {
  std::vector<autofill::CreditCard*> supported_methods;

  std::unordered_set<base::string16> supported_method_types;
  for (web::PaymentMethodData method_data : _paymentRequest.method_data) {
    for (base::string16 supported_method : method_data.supported_methods)
      supported_method_types.insert(supported_method);
  }

  for (autofill::CreditCard* card :
       _personalDataManager->GetCreditCardsToSuggest()) {
    const std::string spec_card_type =
        autofill::data_util::GetPaymentRequestData(card->type())
            .basic_card_payment_type;
    if (supported_method_types.find(base::ASCIIToUTF16(spec_card_type)) !=
        supported_method_types.end())
      supported_methods.push_back(card);
  }

  return supported_methods;
}

- (void)sendPaymentResponse {
  DCHECK(_selectedPaymentMethod);

  // TODO(crbug.com/602666): Unmask if this is a server card and/or ask the user
  //   for CVC here.
  // TODO(crbug.com/602666): Record the use of this card with the
  //   PersonalDataManager.
  web::PaymentResponse paymentResponse;
  paymentResponse.details.cardholder_name =
      _selectedPaymentMethod->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL);
  paymentResponse.details.card_number =
      _selectedPaymentMethod->GetRawInfo(autofill::CREDIT_CARD_NUMBER);
  paymentResponse.details.expiry_month =
      _selectedPaymentMethod->GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH);
  paymentResponse.details.expiry_year = _selectedPaymentMethod->GetRawInfo(
      autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR);
  paymentResponse.details.card_security_code =
      _selectedPaymentMethod->GetRawInfo(
          autofill::CREDIT_CARD_VERIFICATION_CODE);
  if (!_selectedPaymentMethod->billing_address_id().empty()) {
    autofill::AutofillProfile* address = _personalDataManager->GetProfileByGUID(
        _selectedPaymentMethod->billing_address_id());
    if (address) {
      paymentResponse.details.billing_address.country =
          address->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY);
      paymentResponse.details.billing_address.address_line.push_back(
          address->GetRawInfo(autofill::ADDRESS_HOME_LINE1));
      paymentResponse.details.billing_address.address_line.push_back(
          address->GetRawInfo(autofill::ADDRESS_HOME_LINE2));
      paymentResponse.details.billing_address.address_line.push_back(
          address->GetRawInfo(autofill::ADDRESS_HOME_LINE3));
      paymentResponse.details.billing_address.region =
          address->GetRawInfo(autofill::ADDRESS_HOME_STATE);
      paymentResponse.details.billing_address.city =
          address->GetRawInfo(autofill::ADDRESS_HOME_CITY);
      paymentResponse.details.billing_address.dependent_locality =
          address->GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY);
      paymentResponse.details.billing_address.postal_code =
          address->GetRawInfo(autofill::ADDRESS_HOME_ZIP);
      paymentResponse.details.billing_address.sorting_code =
          address->GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE);
      paymentResponse.details.billing_address.language_code =
          base::UTF8ToUTF16(address->language_code());
      paymentResponse.details.billing_address.organization =
          address->GetRawInfo(autofill::COMPANY_NAME);
      paymentResponse.details.billing_address.recipient =
          address->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                           GetApplicationContext()->GetApplicationLocale());
      paymentResponse.details.billing_address.phone =
          address->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER);
    }
  }

  [_delegate paymentRequestCoordinatorDidConfirm:paymentResponse];
}

#pragma mark - PaymentRequestViewControllerDelegate

- (void)paymentRequestViewControllerDidCancel {
  [_delegate paymentRequestCoordinatorDidCancel];
}

- (void)paymentRequestViewControllerDidConfirm {
  [self sendPaymentResponse];
}

- (void)paymentRequestViewControllerDisplayPaymentItems {
  _itemsDisplayCoordinator.reset([[PaymentItemsDisplayCoordinator alloc]
      initWithBaseViewController:_viewController]);
  [_itemsDisplayCoordinator setTotal:_paymentRequest.details.total];
  [_itemsDisplayCoordinator
      setPaymentItems:_paymentRequest.details.display_items];
  [_itemsDisplayCoordinator
      setPayButtonEnabled:(_selectedPaymentMethod != nil)];
  [_itemsDisplayCoordinator setDelegate:self];

  [_itemsDisplayCoordinator start];
}

- (void)paymentRequestViewControllerSelectShippingAddress {
  _shippingAddressSelectionCoordinator.reset(
      [[ShippingAddressSelectionCoordinator alloc]
          initWithBaseViewController:_viewController]);
  const std::vector<autofill::AutofillProfile*> addresses =
      _personalDataManager->GetProfilesToSuggest();
  [_shippingAddressSelectionCoordinator setShippingAddresses:addresses];
  [_shippingAddressSelectionCoordinator
      setSelectedShippingAddress:_selectedShippingAddress];
  [_shippingAddressSelectionCoordinator setDelegate:self];

  [_shippingAddressSelectionCoordinator start];
}

- (void)paymentRequestViewControllerSelectShippingOption {
  _shippingOptionSelectionCoordinator.reset(
      [[ShippingOptionSelectionCoordinator alloc]
          initWithBaseViewController:_viewController]);

  std::vector<web::PaymentShippingOption*> shippingOptions;
  shippingOptions.reserve(_paymentRequest.details.shipping_options.size());
  std::transform(std::begin(_paymentRequest.details.shipping_options),
                 std::end(_paymentRequest.details.shipping_options),
                 std::back_inserter(shippingOptions),
                 [](web::PaymentShippingOption& option) { return &option; });

  [_shippingOptionSelectionCoordinator setShippingOptions:shippingOptions];
  [_shippingOptionSelectionCoordinator
      setSelectedShippingOption:_selectedShippingOption];
  [_shippingOptionSelectionCoordinator setDelegate:self];

  [_shippingOptionSelectionCoordinator start];
}

- (void)paymentRequestViewControllerSelectPaymentMethod {
  _methodSelectionCoordinator.reset([[PaymentMethodSelectionCoordinator alloc]
      initWithBaseViewController:_viewController]);
  [_methodSelectionCoordinator setPaymentMethods:[self supportedMethods]];
  [_methodSelectionCoordinator setSelectedPaymentMethod:_selectedPaymentMethod];
  [_methodSelectionCoordinator setDelegate:self];

  [_methodSelectionCoordinator start];
}

#pragma mark - PaymentItemsDisplayCoordinatorDelegate

- (void)paymentItemsDisplayCoordinatorDidReturn:
    (PaymentItemsDisplayCoordinator*)coordinator {
  [_itemsDisplayCoordinator stop];
  _itemsDisplayCoordinator.reset();
}

- (void)paymentItemsDisplayCoordinatorDidConfirm:
    (PaymentItemsDisplayCoordinator*)coordinator {
  [self sendPaymentResponse];
}

#pragma mark - ShippingAddressSelectionCoordinatorDelegate

- (void)shippingAddressSelectionCoordinator:
            (ShippingAddressSelectionCoordinator*)coordinator
                    selectedShippingAddress:
                        (autofill::AutofillProfile*)shippingAddress {
  _selectedShippingAddress = shippingAddress;
  [_viewController updateSelectedShippingAddress:shippingAddress];

  [_shippingAddressSelectionCoordinator stop];
  _shippingAddressSelectionCoordinator.reset();
}

- (void)shippingAddressSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {
  [_shippingAddressSelectionCoordinator stop];
  _shippingAddressSelectionCoordinator.reset();
}

#pragma mark - ShippingOptionSelectionCoordinatorDelegate

- (void)shippingOptionSelectionCoordinator:
            (ShippingOptionSelectionCoordinator*)coordinator
                    selectedShippingOption:
                        (web::PaymentShippingOption*)shippingOption {
  _selectedShippingOption = shippingOption;
  [_viewController updateSelectedShippingOption:shippingOption];

  [_shippingOptionSelectionCoordinator stop];
  _shippingOptionSelectionCoordinator.reset();
}

- (void)shippingOptionSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {
  [_shippingOptionSelectionCoordinator stop];
  _shippingOptionSelectionCoordinator.reset();
}

#pragma mark - PaymentMethodSelectionCoordinatorDelegate

- (void)paymentMethodSelectionCoordinator:
            (PaymentMethodSelectionCoordinator*)coordinator
                    selectedPaymentMethod:(autofill::CreditCard*)creditCard {
  _selectedPaymentMethod = creditCard;

  [_viewController setSelectedPaymentMethod:creditCard];
  [_viewController loadModel];
  [[_viewController collectionView] reloadData];

  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator.reset();
}

- (void)paymentMethodSelectionCoordinatorDidReturn:
    (PaymentMethodSelectionCoordinator*)coordinator {
  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator.reset();
}

@end
