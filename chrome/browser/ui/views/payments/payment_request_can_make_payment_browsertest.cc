// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestCanMakePaymentQueryTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestCanMakePaymentQueryTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_can_make_payment_query_test.html") {}

  void CallCanMakePayment() {
    ResetEventObserver(DialogEvent::CAN_MAKE_PAYMENT_CALLED);
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryTest);
};

// Visa is required, and user has a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported) {
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"true"});
}

// Pages without a valid SSL cerificate always get "false" from
// .canMakePayment().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_InvalidSSL) {
  SetInvalidSsl();

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"false"});
}

// Visa is required, user has a visa instrument, and user is in incognito
// mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported_InIncognitoMode) {
  SetIncognito();

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"true"});
}

// Visa is required, and user doesn't have a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported) {
  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();

  ExpectBodyContains({"false"});
}

// Visa is required, user doesn't have a visa instrument and the user is in
// incognito mode. In this case canMakePayment always returns true.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_NotSupported_InIncognitoMode) {
  SetIncognito();

  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();

  // Returns true because the user is in incognito mode, even though it should
  // return false in a normal profile.
  ExpectBodyContains({"true"});
}

}  // namespace payments
