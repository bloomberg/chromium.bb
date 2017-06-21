// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_coordinator.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/credit_card_edit_mediator.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillUITypeFromAutofillType;
using ::AutofillTypeFromAutofillUIType;

// Returns true if |card_number| is a supported card type and a valid credit
// card number and no other credit card with the same number exists.
// |error_message| can't be null and will be filled with the appropriate error
// message iff the return value is false.
bool IsValidCreditCardNumber(const base::string16& card_number,
                             const PaymentRequest* payment_request,
                             const autofill::CreditCard* credit_card_to_edit,
                             base::string16* error_message) {
  std::set<std::string> supported_card_networks(
      payment_request->supported_card_networks().begin(),
      payment_request->supported_card_networks().end());
  if (!::autofill::IsValidCreditCardNumberForBasicCardNetworks(
          card_number, supported_card_networks, error_message)) {
    return false;
  }

  // Check if another credit card has already been created with this number.
  // TODO(crbug.com/725604): the UI should offer to load / update the existing
  // credit card info.
  autofill::CreditCard* existing_card =
      payment_request->GetPersonalDataManager()->GetCreditCardByNumber(
          base::UTF16ToASCII(card_number));
  // If a card exists, it could be the one currently being edited.
  if (!existing_card || (credit_card_to_edit && credit_card_to_edit->guid() ==
                                                    existing_card->guid())) {
    return true;
  }
  if (error_message) {
    *error_message = l10n_util::GetStringUTF16(
        IDS_PAYMENTS_VALIDATION_ALREADY_USED_CREDIT_CARD_NUMBER);
  }
  return false;
}

}  // namespace

@interface CreditCardEditCoordinator ()

@property(nonatomic, strong)
    BillingAddressSelectionCoordinator* billingAddressSelectionCoordinator;

@property(nonatomic, strong) UINavigationController* viewController;

@property(nonatomic, strong)
    PaymentRequestEditViewController* editViewController;

@property(nonatomic, strong) CreditCardEditViewControllerMediator* mediator;

@end

@implementation CreditCardEditCoordinator

@synthesize creditCard = _creditCard;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize billingAddressSelectionCoordinator =
    _billingAddressSelectionCoordinator;
@synthesize viewController = _viewController;
@synthesize editViewController = _editViewController;
@synthesize mediator = _mediator;

- (void)start {
  _editViewController = [[PaymentRequestEditViewController alloc] init];
  // TODO(crbug.com/602666): Title varies depending on the missing fields.
  NSString* title = _creditCard
                        ? l10n_util::GetNSString(IDS_PAYMENTS_EDIT_CARD)
                        : l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD_LABEL);
  [_editViewController setTitle:title];
  [_editViewController setDelegate:self];
  [_editViewController setValidatorDelegate:self];
  _mediator = [[CreditCardEditViewControllerMediator alloc]
      initWithPaymentRequest:_paymentRequest
                  creditCard:_creditCard];
  [_mediator setConsumer:_editViewController];
  [_editViewController setDataSource:_mediator];
  [_editViewController loadModel];

  self.viewController = [[UINavigationController alloc]
      initWithRootViewController:self.editViewController];
  [self.viewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self.viewController
      setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  [self.viewController setNavigationBarHidden:YES];

  [[self baseViewController] presentViewController:self.viewController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [[self.viewController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  [self.billingAddressSelectionCoordinator stop];
  self.billingAddressSelectionCoordinator = nil;
  self.editViewController = nil;
  self.viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length) {
    base::string16 errorMessage;
    base::string16 valueString = base::SysNSStringToUTF16(field.value);
    if (field.autofillUIType == AutofillUITypeCreditCardNumber) {
      ::IsValidCreditCardNumber(valueString, _paymentRequest, _creditCard,
                                &errorMessage);
    } else if (field.autofillUIType != AutofillUITypeCreditCardBillingAddress &&
               field.autofillUIType != AutofillUITypeCreditCardSaveToChrome) {
      autofill::IsValidForType(
          valueString, AutofillTypeFromAutofillUIType(field.autofillUIType),
          &errorMessage);
    }
    return !errorMessage.empty() ? base::SysUTF16ToNSString(errorMessage) : nil;
  } else if (field.isRequired) {
    return l10n_util::GetNSString(
        IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE);
  }
  return nil;
}

#pragma mark - PaymentRequestEditViewControllerDelegate

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                          didSelectField:(EditorField*)field {
  if (field.autofillUIType == AutofillUITypeCreditCardBillingAddress) {
    self.billingAddressSelectionCoordinator =
        [[BillingAddressSelectionCoordinator alloc]
            initWithBaseViewController:self.editViewController];
    [self.billingAddressSelectionCoordinator
        setPaymentRequest:self.paymentRequest];
    [self.billingAddressSelectionCoordinator
        setSelectedBillingProfile:self.mediator.billingProfile];
    [self.billingAddressSelectionCoordinator setDelegate:self];
    [self.billingAddressSelectionCoordinator start];
  }
}

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                  didFinishEditingFields:(NSArray<EditorField*>*)fields {
  BOOL saveCreditCard = NO;
  // Create an empty credit card. If a credit card is being edited, copy over
  // the information.
  autofill::CreditCard creditCard =
      _creditCard ? *_creditCard
                  : autofill::CreditCard(base::GenerateGUID(),
                                         autofill::kSettingsOrigin);

  for (EditorField* field in fields) {
    if (field.autofillUIType == AutofillUITypeCreditCardSaveToChrome) {
      saveCreditCard = [field.value boolValue];
    } else if (field.autofillUIType == AutofillUITypeCreditCardBillingAddress) {
      creditCard.set_billing_address_id(base::SysNSStringToUTF8(field.value));
    } else {
      creditCard.SetInfo(autofill::AutofillType(AutofillTypeFromAutofillUIType(
                             field.autofillUIType)),
                         base::SysNSStringToUTF16(field.value),
                         GetApplicationContext()->GetApplicationLocale());
    }
  }

  if (!_creditCard) {
    if (saveCreditCard)
      _paymentRequest->GetPersonalDataManager()->AddCreditCard(creditCard);

    // Add the credit card to the list of credit cards in |_paymentRequest|.
    _creditCard = _paymentRequest->AddCreditCard(creditCard);
  } else {
    // Override the origin.
    creditCard.set_origin(autofill::kSettingsOrigin);

    // Update the original credit card instance that is being edited.
    *_creditCard = creditCard;

    if (autofill::IsCreditCardLocal(creditCard)) {
      _paymentRequest->GetPersonalDataManager()->UpdateCreditCard(creditCard);
    } else {
      // Update server credit card's billing address.
      _paymentRequest->GetPersonalDataManager()->UpdateServerCardMetadata(
          creditCard);
    }
  }

  [_delegate creditCardEditCoordinator:self
            didFinishEditingCreditCard:_creditCard];
}

- (void)paymentRequestEditViewControllerDidCancel:
    (PaymentRequestEditViewController*)controller {
  [_delegate creditCardEditCoordinatorDidCancel:self];
}

#pragma mark - BillingAddressSelectionCoordinatorDelegate

- (void)billingAddressSelectionCoordinator:
            (BillingAddressSelectionCoordinator*)coordinator
                   didSelectBillingAddress:
                       (autofill::AutofillProfile*)billingAddress {
  // Update view controller's data source with the selection and reload the view
  // controller.
  DCHECK(billingAddress);
  [self.mediator setBillingProfile:billingAddress];
  [self.editViewController loadModel];
  [self.editViewController.collectionView reloadData];

  [self.billingAddressSelectionCoordinator stop];
  self.billingAddressSelectionCoordinator = nil;
}

- (void)billingAddressSelectionCoordinatorDidReturn:
    (BillingAddressSelectionCoordinator*)coordinator {
  [self.billingAddressSelectionCoordinator stop];
  self.billingAddressSelectionCoordinator = nil;
}

@end
