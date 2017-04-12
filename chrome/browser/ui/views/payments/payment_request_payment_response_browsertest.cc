// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestPaymentResponseAutofillPaymentInstrumentTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentResponseAutofillPaymentInstrumentTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestPaymentResponseAutofillPaymentInstrumentTest);
};

// Tests that the PaymentResponse contains all the required fields for an
// Autofill payment instrument.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestPaymentResponseAutofillPaymentInstrumentTest,
    TestPaymentResponse) {
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Test that the card details were sent to the merchant.
  ExpectBodyContains(std::vector<base::string16>{
      base::UTF8ToUTF16("\"cardNumber\": \"4111111111111111\""),
      base::UTF8ToUTF16("\"cardSecurityCode\": \"123\""),
      base::UTF8ToUTF16("\"cardholderName\": \"Test User\""),
      base::UTF8ToUTF16("\"expiryMonth\": \"11\""),
      base::UTF8ToUTF16("\"expiryYear\": \"2022\"")});

  // Test that the billing address was sent to the merchant.
  ExpectBodyContains(std::vector<base::string16>{
      base::UTF8ToUTF16("\"billingAddress\": {"),
      base::UTF8ToUTF16("\"666 Erebus St.\""), base::UTF8ToUTF16("\"Apt 8\""),
      base::UTF8ToUTF16("\"city\": \"Elysium\""),
      base::UTF8ToUTF16("\"country\": \"US\""),
      base::UTF8ToUTF16("\"organization\": \"Underworld\""),
      base::UTF8ToUTF16("\"phone\": \"16502111111\""),
      base::UTF8ToUTF16("\"postalCode\": \"91111\""),
      base::UTF8ToUTF16("\"recipient\": \"John H. Doe\""),
      base::UTF8ToUTF16("\"region\": \"CA\"")});
}

class PaymentRequestPaymentResponseShippingAddressTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentResponseShippingAddressTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestPaymentResponseShippingAddressTest);
};

// Tests that the PaymentResponse contains all the required fields for a
// shipping address and shipping option.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentResponseShippingAddressTest,
                       TestPaymentResponse) {
  // Create a billing address and a card that uses it.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Create a shipping address with a higher frecency score, so that it is
  // selected as the defautl shipping address.
  autofill::AutofillProfile shipping_address =
      autofill::test::GetFullProfile2();
  shipping_address.set_use_count(2000);
  AddAutofillProfile(shipping_address);

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Test that the shipping address was sent to the merchant.
  ExpectBodyContains(std::vector<base::string16>{
      base::UTF8ToUTF16("\"country\": \"US\""),
      base::UTF8ToUTF16("\"123 Main Street\""), base::UTF8ToUTF16("\"Unit 1\""),
      base::UTF8ToUTF16("\"region\": \"MI\""),
      base::UTF8ToUTF16("\"city\": \"Greensdale\""),
      base::UTF8ToUTF16("\"dependentLocality\": \"\""),
      base::UTF8ToUTF16("\"postalCode\": \"48838\""),
      base::UTF8ToUTF16("\"sortingCode\": \"\""),
      base::UTF8ToUTF16("\"languageCode\": \"\""),
      base::UTF8ToUTF16("\"organization\": \"ACME\""),
      base::UTF8ToUTF16("\"recipient\": \"Jane A. Smith\""),
      base::UTF8ToUTF16("\"phone\": \"13105557889\"")});

  // Test that the shipping option was sent to the merchant.
  ExpectBodyContains(std::vector<base::string16>{
      base::UTF8ToUTF16("\"shippingOption\": \"freeShippingOption\"")});
}

class PaymentRequestPaymentResponseAllContactDetailsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentResponseAllContactDetailsTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_contact_details_and_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestPaymentResponseAllContactDetailsTest);
};

// Tests that the PaymentResponse contains all the required fields for contact
// details when all three details are requested.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentResponseAllContactDetailsTest,
                       TestPaymentResponse) {
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Test that the contact details were sent to the merchant.
  ExpectBodyContains(std::vector<base::string16>{
      base::UTF8ToUTF16("\"payerName\": \"John H. Doe\""),
      base::UTF8ToUTF16("\"payerEmail\": \"johndoe@hades.com\""),
      base::UTF8ToUTF16("\"payerPhone\": \"+16502111111\"")});
}

class PaymentRequestPaymentResponseOneContactDetailTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentResponseOneContactDetailTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_email_and_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestPaymentResponseOneContactDetailTest);
};

// Tests that the PaymentResponse contains all the required fields for contact
// details when all ont detail is requested.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentResponseOneContactDetailTest,
                       TestPaymentResponse) {
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Test that the contact details were sent to the merchant.
  ExpectBodyContains(std::vector<base::string16>{
      base::UTF8ToUTF16("\"payerName\": null"),
      base::UTF8ToUTF16("\"payerEmail\": \"johndoe@hades.com\""),
      base::UTF8ToUTF16("\"payerPhone\": null")});
}

}  // namespace payments
