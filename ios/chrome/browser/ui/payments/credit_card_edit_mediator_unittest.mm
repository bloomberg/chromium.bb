// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_mediator.h"

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const base::Time kOct2017 = base::Time::FromDoubleT(1509050356);

}  // namespace

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

// Tests that no validation error should be expected if validating an empty
// field that is not required.
TEST_F(PaymentRequestCreditCardEditMediatorTest, ValidateEmptyField) {
  CreditCardEditViewControllerMediator* mediator =
      [[CreditCardEditViewControllerMediator alloc]
          initWithPaymentRequest:payment_request()
                      creditCard:nil];

  EditorField* field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@""
                    required:NO];
  NSString* validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE(!validationError);
}

// Tests that the appropriate validation error should be expected if validating
// an empty field that is required.
TEST_F(PaymentRequestCreditCardEditMediatorTest, ValidateEmptyRequiredField) {
  CreditCardEditViewControllerMediator* mediator =
      [[CreditCardEditViewControllerMediator alloc]
          initWithPaymentRequest:payment_request()
                      creditCard:nil];

  EditorField* field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@""
                    required:YES];
  NSString* validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE([validationError
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE)]);
}

// Tests that the appropriate validation error should be expected if validating
// a field with an invalid value.
TEST_F(PaymentRequestCreditCardEditMediatorTest, ValidateFieldInvalidValue) {
  CreditCardEditViewControllerMediator* mediator =
      [[CreditCardEditViewControllerMediator alloc]
          initWithPaymentRequest:payment_request()
                      creditCard:nil];

  EditorField* field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeCreditCardNumber
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@"411111111111111"  // Missing one last digit.
                    required:YES];
  NSString* validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE([validationError
      isEqualToString:
          l10n_util::GetNSString(
              IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE)]);

  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kOct2017);

  field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeCreditCardExpDate
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@"09 / 17"  // September 2017.
                    required:YES];
  validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE([validationError
      isEqualToString:
          l10n_util::GetNSString(
              IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED)]);
}

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
