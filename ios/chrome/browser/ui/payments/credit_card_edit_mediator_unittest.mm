// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_mediator.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestCreditCardEditMediatorTest
    : public PaymentRequestUnitTestBase,
      public PlatformTest {
 protected:
  void SetUp() override {
    PaymentRequestUnitTestBase::SetUp();
    AddAutofillProfile(autofill::test::GetFullProfile());
    CreateTestPaymentRequest();
  }

  void TearDown() override { PaymentRequestUnitTestBase::TearDown(); }
};

// Tests that the editor's title is correct in various situations.
TEST_F(PaymentRequestCreditCardEditMediatorTest, Title) {
  // No card, so the title should ask to add a card.
  CreditCardEditViewControllerMediator* mediator =
      [[CreditCardEditViewControllerMediator alloc]
          initWithPaymentRequest:payment_request()
                      creditCard:nil];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD_LABEL)]);

  const autofill::AutofillProfile& billing_address = *profiles()[0];

  // Complete card, to title should prompt to edit the card.
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  autofill::test::SetCreditCardInfo(&credit_card, nullptr, nullptr, nullptr,
                                    nullptr, billing_address.guid());
  mediator = [[CreditCardEditViewControllerMediator alloc]
      initWithPaymentRequest:payment_request()
                  creditCard:&credit_card];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_EDIT_CARD)]);

  // The card's name is missing, so the title should prompt to add the name.
  autofill::test::SetCreditCardInfo(&credit_card, /* name_on_card= */ "",
                                    nullptr, nullptr, nullptr,
                                    billing_address.guid());
  mediator = [[CreditCardEditViewControllerMediator alloc]
      initWithPaymentRequest:payment_request()
                  creditCard:&credit_card];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_NAME_ON_CARD)]);

  // Billing address is also missing, so the title should be generic.
  autofill::test::SetCreditCardInfo(&credit_card, /* name_on_card= */ "",
                                    nullptr, nullptr, nullptr,
                                    /* billing_address_id= */ "");
  mediator = [[CreditCardEditViewControllerMediator alloc]
      initWithPaymentRequest:payment_request()
                  creditCard:&credit_card];
  EXPECT_TRUE(
      [mediator.title isEqualToString:l10n_util::GetNSString(
                                          IDS_PAYMENTS_ADD_MORE_INFORMATION)]);
}
