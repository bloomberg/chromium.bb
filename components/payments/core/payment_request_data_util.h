// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_

#include <set>
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
class PaymentMethodData;

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
    const autofill::AutofillProfile& billing_profile,
    const std::string& app_locale);

// Parse the supported card networks from supportedMethods and  "basic-card"'s
// supportedNetworks. |out_supported_networks| is filled with list of networks
// in the order that they were specified by the merchant.
// |out_basic_card_supported_networks| is a subset of |out_supported_networks|
// that includes all networks that were specified as part of "basic-card". This
// is used to know whether to return the card network name (e.g., "visa") or
// "basic-card" in the PaymentResponse. |method_data.supported_networks| is
// expected to only contain basic-card card network names (the list is at
// https://www.w3.org/Payments/card-network-ids).
void ParseBasicCardSupportedNetworks(
    const std::vector<PaymentMethodData>& method_data,
    std::vector<std::string>* out_supported_networks,
    std::set<std::string>* out_basic_card_supported_networks);

// Returns the phone number from the given |profile| formatted for display.
base::string16 GetFormattedPhoneNumberForDisplay(
    const autofill::AutofillProfile& profile,
    const std::string& locale);

// Formats the given number |phone_number| to
// i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL format
// by using i18n::phonenumbers::PhoneNumberUtil::Format.
std::string FormatPhoneForDisplay(const std::string& phone_number,
                                  const std::string& country_code);

// Formats the given number |phone_number| to
// i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::E164 format by using
// i18n::phonenumbers::PhoneNumberUtil::Format, as defined in the Payment
// Request spec
// (https://w3c.github.io/browser-payment-api/#paymentrequest-updated-algorithm)
std::string FormatPhoneForResponse(const std::string& phone_number,
                                   const std::string& country_code);

// Formats |card_number| for display. For example, "4111111111111111" is
// formatted into "4111 1111 1111 1111". This method does not format masked card
// numbers, which start with a letter.
base::string16 FormatCardNumberForDisplay(const base::string16& card_number);

// Returns a country code to be used when validating this profile. If the
// profile has a valid country code set, it is returned. If not, a country code
// associated with |app_locale| is used as a fallback.
std::string GetCountryCodeWithFallback(const autofill::AutofillProfile* profile,
                                       const std::string& app_locale);

}  // namespace data_util
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_DATA_UTIL_H_
