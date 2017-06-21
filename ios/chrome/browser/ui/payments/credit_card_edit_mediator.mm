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
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/payments/cells/accepted_payment_methods_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::autofill::data_util::GetIssuerNetworkForBasicCardIssuerNetwork;
using ::autofill::data_util::GetPaymentRequestData;
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;
}  // namespace

@interface CreditCardEditViewControllerMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The credit card to be edited, if any. This pointer is not owned by this class
// and should outlive it.
@property(nonatomic, assign) autofill::CreditCard* creditCard;

// The map of autofill types to the cached editor fields. Helps reuse the editor
// fields and therefore maintain their existing values when the billing address
// changes and the fields get updated.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, EditorField*>* fieldsMap;

// The reference to the autofill::CREDIT_CARD_EXP_MONTH field, if any (i.e if
// we are dealing with a server card this will not exist).
@property(nonatomic, strong) EditorField* creditCardExpMonthField;

// The reference to the autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR field, if any
// (i.e. if we are dealing with a server card this will not exist).
@property(nonatomic, strong) EditorField* creditCardExpYearField;

@end

@implementation CreditCardEditViewControllerMediator

@synthesize state = _state;
@synthesize consumer = _consumer;
@synthesize billingProfile = _billingProfile;
@synthesize paymentRequest = _paymentRequest;
@synthesize creditCard = _creditCard;
@synthesize fieldsMap = _fieldsMap;
@synthesize creditCardExpMonthField = _creditCardExpMonthField;
@synthesize creditCardExpYearField = _creditCardExpYearField;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
                            creditCard:(autofill::CreditCard*)creditCard {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _creditCard = creditCard;
    _state = _creditCard ? EditViewControllerStateEdit
                         : EditViewControllerStateCreate;
    _billingProfile =
        _creditCard && !_creditCard->billing_address_id().empty()
            ? autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
                  _creditCard->billing_address_id(),
                  _paymentRequest->billing_profiles())
            : nil;
    _fieldsMap = [[NSMutableDictionary alloc] init];
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;

  [self.consumer setEditorFields:[self createEditorFields]];
  if (self.creditCardExpMonthField) {
    [self loadMonths];
  }
  if (self.creditCardExpYearField) {
    [self loadYears];
  }
}

- (void)setBillingProfile:(autofill::AutofillProfile*)billingProfile {
  _billingProfile = billingProfile;
  [self.consumer setEditorFields:[self createEditorFields]];
}

#pragma mark - PaymentRequestEditViewControllerDataSource

- (CollectionViewItem*)headerItem {
  if (_creditCard && !autofill::IsCreditCardLocal(*_creditCard)) {
    // Return an item that identifies the server card being edited.
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

  // Otherwise, return an item that displays a list of payment method type icons
  // for the accepted payment methods.
  NSMutableArray* issuerNetworkIcons = [NSMutableArray array];
  for (const auto& supportedNetwork :
       _paymentRequest->supported_card_networks()) {
    const std::string issuerNetwork =
        GetIssuerNetworkForBasicCardIssuerNetwork(supportedNetwork);
    const autofill::data_util::PaymentRequestData& cardData =
        GetPaymentRequestData(issuerNetwork);
    UIImage* issuerNetworkIcon = NativeImage(cardData.icon_resource_id);
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

- (BOOL)shouldHideBackgroundForHeaderItem {
  // YES if the header item displays the accepted payment method type icons.
  return !_creditCard || autofill::IsCreditCardLocal(*_creditCard);
}

- (void)formatValueForEditorField:(EditorField*)field {
  if (field.autofillUIType == AutofillUITypeCreditCardNumber) {
    field.value = base::SysUTF16ToNSString(
        payments::data_util::FormatCardNumberForDisplay(
            base::SysNSStringToUTF16(field.value)));
  }
}

- (UIImage*)iconIdentifyingEditorField:(EditorField*)field {
  // Early return if the field is not the credit card number field.
  if (field.autofillUIType != AutofillUITypeCreditCardNumber)
    return nil;

  const char* issuerNetwork = autofill::CreditCard::GetCardNetwork(
      base::SysNSStringToUTF16(field.value));
  // This should not happen in Payment Request.
  if (issuerNetwork == autofill::kGenericCard)
    return nil;

  // Return the card issuer network icon.
  int resourceID = autofill::data_util::GetPaymentRequestData(issuerNetwork)
                       .icon_resource_id;
  return NativeImage(resourceID);
}

#pragma mark - Helper methods

// Queries the month numbers.
- (void)loadMonths {
  NSMutableArray<NSString*>* months = [[NSMutableArray alloc] init];

  for (int month = 1; month <= 12; month++) {
    NSString* monthString = [NSString stringWithFormat:@"%02d", month];
    [months addObject:monthString];
  }

  // Notify the view controller asynchronously to allow for the view to update.
  __weak CreditCardEditViewControllerMediator* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.consumer setOptions:months
                   forEditorField:weakSelf.creditCardExpMonthField];
  });
}

// Queries the year numbers.
- (void)loadYears {
  NSMutableArray<NSString*>* years = [[NSMutableArray alloc] init];

  NSDateComponents* dateComponents =
      [[NSCalendar currentCalendar] components:NSCalendarUnitYear
                                      fromDate:[NSDate date]];

  int currentYear = [dateComponents year];
  bool foundCardExpirationYear = false;
  for (int year = currentYear; year < currentYear + 10; year++) {
    NSString* yearString = [NSString stringWithFormat:@"%04d", year];
    if ([yearString isEqualToString:_creditCardExpYearField.value])
      foundCardExpirationYear = true;
    [years addObject:yearString];
  }

  // Ensure that the expiration year on a user's saved card is
  // always available as one of the options to select from. This is
  // useful in the case that the user's card is expired.
  if (!foundCardExpirationYear) {
    [years insertObject:_creditCardExpYearField.value atIndex:0];
  }

  // Notify the view controller asynchronously to allow for the view to update.
  __weak CreditCardEditViewControllerMediator* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.consumer setOptions:years
                   forEditorField:weakSelf.creditCardExpYearField];
  });
}

- (NSArray<EditorField*>*)createEditorFields {
  NSMutableArray<EditorField*>* fields = [[NSMutableArray alloc] init];

  // Billing address field.
  NSNumber* fieldKey =
      [NSNumber numberWithInt:AutofillUITypeCreditCardBillingAddress];
  EditorField* billingAddressField = self.fieldsMap[fieldKey];
  if (!billingAddressField) {
    billingAddressField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardBillingAddress
                     fieldType:EditorFieldTypeSelector
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_BILLING_ADDRESS)
                         value:nil
                      required:YES];
    [self.fieldsMap setObject:billingAddressField forKey:fieldKey];
  }
  billingAddressField.value =
      self.billingProfile ? base::SysUTF8ToNSString(self.billingProfile->guid())
                          : nil;
  billingAddressField.displayValue =
      self.billingProfile
          ? GetBillingAddressLabelFromAutofillProfile(*self.billingProfile)
          : nil;

  // Early return if editing a server card. For server cards, only the billing
  // address can be edited.
  if (_creditCard && !autofill::IsCreditCardLocal(*_creditCard))
    return @[ billingAddressField ];

  // Credit Card number field.
  NSString* creditCardNumber =
      _creditCard ? base::SysUTF16ToNSString(
                        payments::data_util::FormatCardNumberForDisplay(
                            _creditCard->number()))
                  : nil;
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardNumber];
  EditorField* creditCardNumberField = self.fieldsMap[fieldKey];
  if (!creditCardNumberField) {
    creditCardNumberField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardNumber
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_PAYMENTS_CARD_NUMBER)
                         value:creditCardNumber
                      required:YES];
    [self.fieldsMap setObject:creditCardNumberField forKey:fieldKey];
  }
  [fields addObject:creditCardNumberField];

  // Card holder name field.
  NSString* creditCardName =
      _creditCard
          ? autofill::GetCreditCardName(
                *_creditCard, GetApplicationContext()->GetApplicationLocale())
          : nil;
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardHolderFullName];
  EditorField* creditCardNameField = self.fieldsMap[fieldKey];
  if (!creditCardNameField) {
    creditCardNameField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardHolderFullName
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_PAYMENTS_NAME_ON_CARD)
                         value:creditCardName
                      required:YES];
    [self.fieldsMap setObject:creditCardNameField forKey:fieldKey];
  }
  [fields addObject:creditCardNameField];

  NSDateComponents* dateComponents = [[NSCalendar currentCalendar]
      components:NSCalendarUnitMonth | NSCalendarUnitYear
        fromDate:[NSDate date]];

  int currentMonth = [dateComponents month];

  // Expiration month field.
  NSString* creditCardExpMonth =
      _creditCard && (_creditCard->expiration_month() >= 1 &&
                      _creditCard->expiration_month() <= 12)
          ? [NSString stringWithFormat:@"%02d", _creditCard->expiration_month()]
          : [NSString stringWithFormat:@"%02d", currentMonth];
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardExpMonth];
  EditorField* expirationMonthField = self.fieldsMap[fieldKey];
  if (!expirationMonthField) {
    expirationMonthField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardExpMonth
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_PAYMENTS_EXP_MONTH)
                         value:creditCardExpMonth
                      required:YES];
    [self.fieldsMap setObject:expirationMonthField forKey:fieldKey];
  }
  self.creditCardExpMonthField = expirationMonthField;
  [fields addObject:expirationMonthField];

  // Expiration year field.
  int currentYear = [dateComponents year];
  NSString* creditCardExpYear =
      _creditCard
          ? [NSString stringWithFormat:@"%04d", _creditCard->expiration_year()]
          : [NSString stringWithFormat:@"%04d", currentYear];
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardExpYear];
  EditorField* expirationYearField = self.fieldsMap[fieldKey];
  if (!expirationYearField) {
    expirationYearField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardExpYear
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_PAYMENTS_EXP_YEAR)
                         value:creditCardExpYear
                      required:YES];
    [self.fieldsMap setObject:expirationYearField forKey:fieldKey];
  }
  self.creditCardExpYearField = expirationYearField;
  [fields addObject:expirationYearField];

  // The billing address field appears after the expiration year field.
  [fields addObject:billingAddressField];

  // Save credit card field.
  fieldKey = [NSNumber numberWithInt:AutofillUITypeCreditCardSaveToChrome];
  EditorField* saveToChromeField = self.fieldsMap[fieldKey];
  if (!saveToChromeField) {
    saveToChromeField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardSaveToChrome
                     fieldType:EditorFieldTypeSwitch
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_SAVE_CARD_TO_DEVICE_CHECKBOX)
                         value:@"YES"
                      required:YES];
    [self.fieldsMap setObject:saveToChromeField forKey:fieldKey];
  }
  [fields addObject:saveToChromeField];

  return fields;
}

@end
