// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace payments {

struct BasicCardResponse;
struct PaymentAddress;

namespace data_util {

// Helper function to get an instance of web::PaymentAddress from an autofill
// profile.
PaymentAddress GetPaymentAddressFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& app_locale);

// Helper function to get an instance of web::BasicCardResponse from an autofill
// credit card.
BasicCardResponse GetBasicCardResponseFromAutofillCreditCard(
    const autofill::CreditCard& card,
    const base::string16& cvc,
    const std::vector<autofill::AutofillProfile*>& billing_profiles,
    const std::string& app_locale);

}  // namespace data_util
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
