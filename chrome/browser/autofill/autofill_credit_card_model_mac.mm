// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/credit_card.h"
#include "grit/generated_resources.h"


@implementation AutoFillCreditCardModel

@dynamic summary;
@synthesize label = label_;
@synthesize nameOnCard = nameOnCard_;
@synthesize creditCardNumber = creditCardNumber_;
@synthesize expirationMonth = expirationMonth_;
@synthesize expirationYear = expirationYear_;
@synthesize cvcCode = cvcCode_;
@synthesize billingAddress = billingAddress_;
@synthesize shippingAddress = shippingAddress_;

// Sets up the KVO dependency between "summary" and dependent fields.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* keyPaths = [super keyPathsForValuesAffectingValueForKey:key];

  if ([key isEqualToString:@"summary"]) {
    NSSet* affectingKeys = [NSSet setWithObjects:@"creditCardNumber",
                            @"expirationMonth", @"expirationYear", nil];
    keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKeys];
  }
  return keyPaths;
}

- (id)initWithCreditCard:(const CreditCard&)creditCard {
  if ((self = [super init])) {
    [self setLabel:SysUTF16ToNSString(creditCard.Label())];
    [self setNameOnCard:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_NAME)))];
    [self setCreditCardNumber:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_NUMBER)))];
    [self setExpirationMonth:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)))];
    [self setExpirationYear:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)))];
    [self setCvcCode:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_VERIFICATION_CODE)))];
    [self setBillingAddress:SysUTF16ToNSString(
        creditCard.billing_address())];
    [self setShippingAddress:SysUTF16ToNSString(
        creditCard.shipping_address())];
  }
  return self;
}

- (void)dealloc {
  [label_ release];
  [nameOnCard_ release];
  [creditCardNumber_ release];
  [expirationMonth_ release];
  [expirationYear_ release];
  [cvcCode_ release];
  [billingAddress_ release];
  [shippingAddress_ release];
  [super dealloc];
}

- (NSString*)summary {
  // Create a temporary |creditCard| to generate summary string.
  CreditCard creditCard(string16(), 0);
  [self copyModelToCreditCard:&creditCard];
  return SysUTF16ToNSString(creditCard.PreviewSummary());
}

- (void)copyModelToCreditCard:(CreditCard*)creditCard {
  DCHECK(creditCard);
  creditCard->set_label(base::SysNSStringToUTF16([self label]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_NAME),
      base::SysNSStringToUTF16([self nameOnCard]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
      base::SysNSStringToUTF16([self creditCardNumber]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
      base::SysNSStringToUTF16([self expirationMonth]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
      base::SysNSStringToUTF16([self expirationYear]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_VERIFICATION_CODE),
      base::SysNSStringToUTF16([self cvcCode]));
  creditCard->set_billing_address(
      base::SysNSStringToUTF16([self billingAddress]));
  creditCard->set_shipping_address(
      base::SysNSStringToUTF16([self shippingAddress]));
}

@end
