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
    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
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

// Pages without a valid SSL certificate always get "false" from
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

class PaymentRequestCanMakePaymentQueryCCTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestCanMakePaymentQueryCCTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_can_make_payment_query_cc_test.html") {}

  // If |visa| is true, then the method data is:
  //
  //   [{supportedMethods: ['visa']}]
  //
  // If |visa| is false, then the method data is:
  //
  //   [{supportedMethods: ['mastercard']}]
  void CallCanMakePayment(bool visa) {
    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       visa ? "buy();" : "other_buy();"));
    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryCCTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest, QueryQuota) {
  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  // User does not have a visa card.
  ExpectBodyContains({"false"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  // User now has a visa card. The query is cached, but the result is always
  // fresh.
  ExpectBodyContains({"true"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});
}

// canMakePayment() always returns true for credit cards in incognito mode,
// regardless of the query quota.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryCCTest,
                       NoQueryQuotaInIncognito) {
  SetIncognito();

  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  ExpectBodyContains({"true"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  ExpectBodyContains({"true"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "visa" payment method.
  CallCanMakePayment(/*visa=*/true);

  ExpectBodyContains({"true"});

  // Query "mastercard" payment method.
  CallCanMakePayment(/*visa=*/false);

  ExpectBodyContains({"true"});
}

class PaymentRequestCanMakePaymentQueryPMITest
    : public PaymentRequestBrowserTestBase {
 protected:
  enum class CheckFor {
    BASIC_VISA,
    BASIC_CARD,
    ALICE_PAY,
    BOB_PAY,
    BOB_PAY_AND_BASIC_CARD,
    BOB_PAY_AND_VISA,
  };

  PaymentRequestCanMakePaymentQueryPMITest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_payment_method_identifier_test.html") {
    script_[CheckFor::BASIC_VISA] = "checkBasicVisa();";
    script_[CheckFor::BASIC_CARD] = "checkBasicCard();";
    script_[CheckFor::ALICE_PAY] = "checkAlicePay();";
    script_[CheckFor::BOB_PAY] = "checkBobPay();";
    script_[CheckFor::BOB_PAY_AND_BASIC_CARD] = "checkBobPayAndBasicCard();";
    script_[CheckFor::BOB_PAY_AND_VISA] = "checkBobPayAndVisa();";
  }

  void CallCanMakePayment(CheckFor check_for) {
    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), script_[check_for]));
    WaitForObservedEvent();
  }

 private:
  std::map<CheckFor, std::string> script_;
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentQueryPMITest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForBasicCards) {
  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  // User does not have a visa card.
  ExpectBodyContains({"false"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  // User now has a visa card. The query is cached, but the result is always
  // fresh.
  ExpectBodyContains({"true"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  // Query quota exceeded.
  ExpectBodyContains({"NotAllowedError"});
}

// canMakePayment() always returns true for credit cards in incognito mode,
// regardless of the query quota.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       NoQueryQuotaForBasicCardsInIncognito) {
  SetIncognito();

  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  ExpectBodyContains({"true"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  ExpectBodyContains({"true"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  // Query "basic-card" payment method with "supportedNetworks": ["visa"] in the
  // payment method specific data.
  CallCanMakePayment(CheckFor::BASIC_VISA);

  ExpectBodyContains({"true"});

  // Query "basic-card" payment method without "supportedNetworks" parameter.
  CallCanMakePayment(CheckFor::BASIC_CARD);

  ExpectBodyContains({"true"});
}

// If the device does not have any payment apps installed, canMakePayment()
// should return false for them.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForPaymentApps) {
  CallCanMakePayment(CheckFor::ALICE_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);

  ExpectBodyContains({"NotAllowedError"});

  CallCanMakePayment(CheckFor::ALICE_PAY);

  ExpectBodyContains({"false"});
}

// If the device does not have any payment apps installed, canMakePayment()
// queries for both payment apps and basic-card depend only on what cards the
// user has on file.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       QueryQuotaForPaymentAppsAndCards) {
  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"NotAllowedError"});
}

// If the device does not have any payment apps installed, canMakePayment()
// should return false for them in the incognito mode regardless of the query
// quota.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       NoQueryQuotaForPaymentAppsInIncognitoMode) {
  SetIncognito();

  CallCanMakePayment(CheckFor::ALICE_PAY);

  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);

  ExpectBodyContains({"false"});
}

// If the device does not have any payment apps installed, canMakePayment()
// queries for both payment apps and basic-card always return true, regardless
// of the query quota.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryPMITest,
                       NoQueryQuotaForPaymentAppsAndCardsInIncognito) {
  SetIncognito();

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"true"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"true"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);

  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);

  ExpectBodyContains({"true"});
}

}  // namespace payments
