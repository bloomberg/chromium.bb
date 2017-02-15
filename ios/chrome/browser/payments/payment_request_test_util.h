// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_TEST_UTIL_H_

#include <memory>

#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

class PaymentRequest;

namespace payment_request_test_util {

// Returns an instance of web::PaymentRequest for testing purposes.
web::PaymentRequest CreateTestWebPaymentRequest();

// Returns an instance of autofill::AutofillProfile for testing purposes.
std::unique_ptr<autofill::AutofillProfile> CreateTestAutofillProfile();

// Returns an instance of autofill::CreditCard for testing purposes.
std::unique_ptr<autofill::CreditCard> CreateTestCreditCard();

// Returns an instance of PaymentRequest with one autofill profile and one
// credit card for testing purposes.
std::unique_ptr<PaymentRequest> CreateTestPaymentRequest();

}  // namespace payment_request_test_util

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_TEST_UTIL_H_
