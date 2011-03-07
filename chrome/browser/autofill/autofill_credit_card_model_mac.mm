// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#include "chrome/browser/autofill/credit_card.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"


@implementation AutoFillCreditCardModel

@synthesize nameOnCard = nameOnCard_;
@synthesize creditCardNumber = creditCardNumber_;
@synthesize expirationMonth = expirationMonth_;
@synthesize expirationYear = expirationYear_;

- (id)initWithCreditCard:(const CreditCard&)creditCard {
  if ((self = [super init])) {
    [self setNameOnCard:SysUTF16ToNSString(
        creditCard.GetFieldText(AutofillType(CREDIT_CARD_NAME)))];
    [self setCreditCardNumber:SysUTF16ToNSString(
        creditCard.GetFieldText(AutofillType(CREDIT_CARD_NUMBER)))];
    [self setExpirationMonth:SysUTF16ToNSString(
        creditCard.GetFieldText(AutofillType(CREDIT_CARD_EXP_MONTH)))];
    [self setExpirationYear:SysUTF16ToNSString(
        creditCard.GetFieldText(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)))];
  }
  return self;
}

- (void)dealloc {
  [nameOnCard_ release];
  [creditCardNumber_ release];
  [expirationMonth_ release];
  [expirationYear_ release];
  [super dealloc];
}

- (void)copyModelToCreditCard:(CreditCard*)creditCard {
  DCHECK(creditCard);
  creditCard->SetInfo(AutofillType(CREDIT_CARD_NAME),
      base::SysNSStringToUTF16([self nameOnCard]));
  creditCard->SetInfo(AutofillType(CREDIT_CARD_NUMBER),
      base::SysNSStringToUTF16([self creditCardNumber]));
  creditCard->SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
      base::SysNSStringToUTF16([self expirationMonth]));
  creditCard->SetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
      base::SysNSStringToUTF16([self expirationYear]));
}

@end
