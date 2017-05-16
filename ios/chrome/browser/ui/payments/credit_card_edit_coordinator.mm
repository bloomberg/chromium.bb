// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_coordinator.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/strings/grit/components_strings.h"
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
}  // namespace

@interface CreditCardEditCoordinator () {
  CreditCardEditViewController* _viewController;

  CreditCardEditViewControllerMediator* _mediator;
}

@end

@implementation CreditCardEditCoordinator

@synthesize creditCard = _creditCard;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;

- (void)start {
  _viewController = [[CreditCardEditViewController alloc] init];
  // TODO(crbug.com/602666): Title varies depending on the missing fields.
  NSString* title = _creditCard
                        ? l10n_util::GetNSString(IDS_PAYMENTS_EDIT_CARD)
                        : l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD_LABEL);
  [_viewController setTitle:title];
  [_viewController setDelegate:self];
  [_viewController setValidatorDelegate:self];
  _mediator = [[CreditCardEditViewControllerMediator alloc]
      initWithPaymentRequest:_paymentRequest
                  creditCard:_creditCard];
  [_mediator setConsumer:_viewController];
  [_viewController setDataSource:_mediator];
  [_viewController loadModel];

  DCHECK(self.baseViewController.navigationController);
  [[self baseViewController].navigationController
      pushViewController:_viewController
                animated:YES];
}

- (void)stop {
  [_viewController.navigationController popViewControllerAnimated:YES];
  _viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length) {
    base::string16 errorMessage;
    base::string16 valueString = base::SysNSStringToUTF16(field.value);
    if (field.autofillUIType == AutofillUITypeCreditCardNumber) {
      std::set<std::string> supportedCardNetworks(
          _paymentRequest->supported_card_networks().begin(),
          _paymentRequest->supported_card_networks().end());
      autofill::IsValidCreditCardNumberForBasicCardNetworks(
          valueString, supportedCardNetworks, &errorMessage);
    } else if (field.autofillUIType == AutofillUITypeCreditCardBillingAddress) {
      // TODO(crbug.com/602666): More validation?
      return nil;
    } else {
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

#pragma mark - CreditCardEditViewControllerDelegate

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                          didSelectField:(EditorField*)field {
  if (field.autofillUIType == AutofillUITypeCreditCardBillingAddress) {
    // TODO(crbug.com/602666): Display a list of billing addresses.
  }
}

- (void)creditCardEditViewController:(CreditCardEditViewController*)controller
              didFinishEditingFields:(NSArray<EditorField*>*)fields
                      saveCreditCard:(BOOL)saveCreditCard {
  // Create an empty credit card. If a credit card is being edited, copy over
  // the information.
  autofill::CreditCard creditCard("", autofill::kSettingsOrigin);
  if (_creditCard)
    creditCard = *_creditCard;

  for (EditorField* field in fields) {
    if (field.autofillUIType == AutofillUITypeCreditCardBillingAddress) {
      creditCard.set_billing_address_id(base::SysNSStringToUTF8(field.value));
    } else {
      creditCard.SetRawInfo(
          AutofillTypeFromAutofillUIType(field.autofillUIType),
          base::SysNSStringToUTF16(field.value));
    }
  }

  if (!_creditCard) {
    if (saveCreditCard) {
      // The new credit card does not yet have a valid GUID.
      creditCard.set_guid(base::GenerateGUID());
      _paymentRequest->GetPersonalDataManager()->AddCreditCard(creditCard);
    }

    // Add the credit card to the list of credit cards in |_paymentRequest|.
    _creditCard = _paymentRequest->AddCreditCard(creditCard);
  } else {
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

- (void)creditCardEditViewControllerDidCancel:
    (CreditCardEditViewController*)controller {
  [_delegate creditCardEditCoordinatorDidCancel:self];
}

@end
