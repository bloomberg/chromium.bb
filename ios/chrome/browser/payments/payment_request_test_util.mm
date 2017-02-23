// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request_test_util.h"

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/web/public/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payment_request_test_util {

web::PaymentRequest CreateTestWebPaymentRequest() {
  web::PaymentRequest web_payment_request;
  web::PaymentMethodData method_datum;
  method_datum.supported_methods.push_back(base::ASCIIToUTF16("visa"));
  web_payment_request.method_data.push_back(method_datum);
  web_payment_request.details.total.label = base::ASCIIToUTF16("Total");
  web_payment_request.details.total.amount.value = base::ASCIIToUTF16("1.00");
  web_payment_request.details.total.amount.currency = base::ASCIIToUTF16("USD");
  web::PaymentItem display_item;
  display_item.label = base::ASCIIToUTF16("Subtotal");
  display_item.amount.value = base::ASCIIToUTF16("1.00");
  display_item.amount.currency = base::ASCIIToUTF16("USD");
  web_payment_request.details.display_items.push_back(display_item);
  web::PaymentShippingOption shipping_option;
  shipping_option.id = base::ASCIIToUTF16("123456");
  shipping_option.label = base::ASCIIToUTF16("1-Day");
  shipping_option.amount.value = base::ASCIIToUTF16("0.99");
  shipping_option.amount.currency = base::ASCIIToUTF16("USD");
  shipping_option.selected = true;
  web_payment_request.details.shipping_options.push_back(shipping_option);
  web::PaymentShippingOption shipping_option2;
  shipping_option2.id = base::ASCIIToUTF16("654321");
  shipping_option2.label = base::ASCIIToUTF16("10-Days");
  shipping_option2.amount.value = base::ASCIIToUTF16("0.01");
  shipping_option2.amount.currency = base::ASCIIToUTF16("USD");
  shipping_option2.selected = false;
  web_payment_request.details.shipping_options.push_back(shipping_option2);
  web_payment_request.options.request_shipping = true;
  return web_payment_request;
}

std::unique_ptr<autofill::AutofillProfile> CreateTestAutofillProfile() {
  std::unique_ptr<autofill::AutofillProfile> profile =
      base::MakeUnique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  "https://www.example.com");
  autofill::test::SetProfileInfo(
      profile.get(), "John", "Mitchell", "Doe", "johndoe@me.xyz", "Fox",
      "123 Zoo St", "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  return profile;
}

std::unique_ptr<autofill::CreditCard> CreateTestCreditCard() {
  std::unique_ptr<autofill::CreditCard> credit_card =
      base::MakeUnique<autofill::CreditCard>(base::GenerateGUID(),
                                             "https://www.example.com/");
  autofill::test::SetCreditCardInfo(credit_card.get(), "John Doe",
                                    "411111111111" /* Visa */, "01", "2999");
  return credit_card;
}

std::unique_ptr<PaymentRequest> CreateTestPaymentRequest() {
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager =
      base::MakeUnique<autofill::TestPersonalDataManager>();
  // Add testing credit card. autofill::TestPersonalDataManager does not take
  // ownership of the card.
  std::unique_ptr<autofill::CreditCard> credit_card = CreateTestCreditCard();
  personal_data_manager->AddTestingCreditCard(credit_card.get());
  // Add testing profile. autofill::TestPersonalDataManager does not take
  // ownership of the profile.
  std::unique_ptr<autofill::AutofillProfile> profile =
      CreateTestAutofillProfile();
  personal_data_manager->AddTestingProfile(profile.get());

  web::PaymentRequest web_payment_request = CreateTestWebPaymentRequest();

  return base::MakeUnique<PaymentRequest>(
      base::MakeUnique<web::PaymentRequest>(web_payment_request),
      personal_data_manager.get());
}

}  // namespace payment_request_test_util
