// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/cells/accepted_payment_methods_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillUITypeFromAutofillType;
using ::autofill::data_util::GetIssuerNetworkForBasicCardIssuerNetwork;
using ::autofill::data_util::GetPaymentRequestData;
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;

const CGFloat kCardIssuerNetworkIconDimension = 20.0;
}  // namespace

@interface CreditCardEditViewControllerMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The credit card to be edited, if any. This pointer is not owned by this class
// and should outlive it.
@property(nonatomic, assign) autofill::CreditCard* creditCard;

// The list of field definitions for the editor.
@property(nonatomic, strong) NSArray<EditorField*>* editorFields;

@end

@implementation CreditCardEditViewControllerMediator

@synthesize state = _state;
@synthesize paymentRequest = _paymentRequest;
@synthesize creditCard = _creditCard;
@synthesize editorFields = _editorFields;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
                            creditCard:(autofill::CreditCard*)creditCard {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _creditCard = creditCard;
    _editorFields = [self createEditorFields];
  }
  return self;
}

- (CollectionViewItem*)serverCardSummaryItem {
  // Return nil if creating or editing a local card.
  if (!_creditCard || autofill::IsCreditCardLocal(*_creditCard))
    return nil;

  PaymentMethodItem* cardSummaryItem = [[PaymentMethodItem alloc] init];
  cardSummaryItem.methodID =
      base::SysUTF16ToNSString(_creditCard->NetworkAndLastFourDigits());
  cardSummaryItem.methodDetail = base::SysUTF16ToNSString(
      _creditCard->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
  const int issuerNetworkIconID =
      autofill::data_util::GetPaymentRequestData(_creditCard->network())
          .icon_resource_id;
  cardSummaryItem.methodTypeIcon = NativeImage(issuerNetworkIconID);
  return cardSummaryItem;
}

- (CollectionViewItem*)acceptedPaymentMethodsItem {
  // Return nil if a server card is being edited.
  if (_creditCard && !autofill::IsCreditCardLocal(*_creditCard))
    return nil;

  NSMutableArray* issuerNetworkIcons = [NSMutableArray array];
  for (const auto& supportedNetwork :
       _paymentRequest->supported_card_networks()) {
    const std::string issuerNetwork =
        GetIssuerNetworkForBasicCardIssuerNetwork(supportedNetwork);
    const autofill::data_util::PaymentRequestData& cardData =
        GetPaymentRequestData(issuerNetwork);
    UIImage* issuerNetworkIcon =
        ResizeImage(NativeImage(cardData.icon_resource_id),
                    CGSizeMake(kCardIssuerNetworkIconDimension,
                               kCardIssuerNetworkIconDimension),
                    ProjectionMode::kAspectFillNoClipping);
    issuerNetworkIcon.accessibilityLabel =
        l10n_util::GetNSString(cardData.a11y_label_resource_id);
    [issuerNetworkIcons addObject:issuerNetworkIcon];
  }

  AcceptedPaymentMethodsItem* acceptedMethodsItem =
      [[AcceptedPaymentMethodsItem alloc] init];
  acceptedMethodsItem.message =
      l10n_util::GetNSString(IDS_PAYMENTS_ACCEPTED_CARDS_LABEL);
  acceptedMethodsItem.methodTypeIcons = issuerNetworkIcons;
  return acceptedMethodsItem;
}

- (NSString*)billingAddressLabelForProfileWithGUID:(NSString*)profileGUID {
  DCHECK(profileGUID);
  autofill::AutofillProfile* profile =
      autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
          base::SysNSStringToUTF8(profileGUID),
          _paymentRequest->billing_profiles());
  DCHECK(profile);
  return GetBillingAddressLabelFromAutofillProfile(*profile);
}

- (UIImage*)cardTypeIconFromCardNumber:(NSString*)cardNumber {
  const char* issuerNetwork = autofill::CreditCard::GetCardNetwork(
      base::SysNSStringToUTF16(cardNumber));
  // This should not happen in Payment Request.
  if (issuerNetwork == autofill::kGenericCard)
    return nil;

  // Resize and return the card issuer network icon.
  int resourceID = autofill::data_util::GetPaymentRequestData(issuerNetwork)
                       .icon_resource_id;
  CGFloat dimension = kCardIssuerNetworkIconDimension;
  return ResizeImage(NativeImage(resourceID), CGSizeMake(dimension, dimension),
                     ProjectionMode::kAspectFillNoClipping);
}

- (NSArray<EditorField*>*)editorFields {
  return _editorFields;
}

#pragma mark - Helper methods

- (NSArray<EditorField*>*)createEditorFields {
  // Server credit cards are not editable.
  if (_creditCard && !autofill::IsCreditCardLocal(*_creditCard))
    return @[];

  NSString* creditCardNumber =
      _creditCard ? base::SysUTF16ToNSString(_creditCard->number()) : nil;

  NSString* creditCardName =
      _creditCard
          ? autofill::GetCreditCardName(
                *_creditCard, GetApplicationContext()->GetApplicationLocale())
          : nil;

  NSString* creditCardExpMonth =
      _creditCard
          ? [NSString stringWithFormat:@"%02d", _creditCard->expiration_month()]
          : nil;

  NSString* creditCardExpYear =
      _creditCard
          ? [NSString stringWithFormat:@"%04d", _creditCard->expiration_year()]
          : nil;

  return @[
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeFromAutofillType(
                                   autofill::CREDIT_CARD_NUMBER)
                         label:l10n_util::GetNSString(IDS_PAYMENTS_CARD_NUMBER)
                         value:creditCardNumber
                      required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeFromAutofillType(
                                   autofill::CREDIT_CARD_NAME_FULL)
                         label:l10n_util::GetNSString(IDS_PAYMENTS_NAME_ON_CARD)
                         value:creditCardName
                      required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeFromAutofillType(
                                   autofill::CREDIT_CARD_EXP_MONTH)
                         label:l10n_util::GetNSString(IDS_PAYMENTS_EXP_MONTH)
                         value:creditCardExpMonth
                      required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeFromAutofillType(
                                   autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)
                         label:l10n_util::GetNSString(IDS_PAYMENTS_EXP_YEAR)
                         value:creditCardExpYear
                      required:YES]
  ];
}

@end
