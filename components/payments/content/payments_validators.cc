// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payments_validators.h"

#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace payments {

// We limit the maximum length of string to 2048 bytes for security reasons.
static const int maximumStringLength = 2048;

bool PaymentsValidators::isValidCurrencyCodeFormat(
    const std::string& code,
    const std::string& system,
    std::string* optional_error_message) {
  if (system == "urn:iso:std:iso:4217") {
    if (RE2::FullMatch(code, "[A-Z]{3}"))
      return true;

    if (optional_error_message)
      *optional_error_message =
          "'" + code +
          "' is not a valid ISO 4217 currency code, should "
          "be 3 upper case letters [A-Z]";

    return false;
  }

  if (code.size() > maximumStringLength) {
    if (optional_error_message)
      *optional_error_message =
          "The currency code should be at most 2048 characters long";
    return false;
  }
  if (!GURL(system).is_valid()) {
    if (optional_error_message)
      *optional_error_message = "The system should be a valid URL";
    return false;
  }
  return true;
}

bool PaymentsValidators::isValidAmountFormat(
    const std::string& amount,
    std::string* optional_error_message) {
  if (RE2::FullMatch(amount, "-?[0-9]+(\\.[0-9]+)?"))
    return true;

  if (optional_error_message)
    *optional_error_message = "'" + amount + "' is not a valid amount format";

  return false;
}

bool PaymentsValidators::isValidCountryCodeFormat(
    const std::string& code,
    std::string* optional_error_message) {
  if (RE2::FullMatch(code, "[A-Z]{2}"))
    return true;

  if (optional_error_message)
    *optional_error_message = "'" + code +
                              "' is not a valid CLDR country code, should be 2 "
                              "upper case letters [A-Z]";

  return false;
}

bool PaymentsValidators::isValidLanguageCodeFormat(
    const std::string& code,
    std::string* optional_error_message) {
  if (RE2::FullMatch(code, "([a-z]{2,3})?"))
    return true;

  if (optional_error_message)
    *optional_error_message =
        "'" + code +
        "' is not a valid BCP-47 language code, should be "
        "2-3 lower case letters [a-z]";

  return false;
}

bool PaymentsValidators::isValidScriptCodeFormat(
    const std::string& code,
    std::string* optional_error_message) {
  if (RE2::FullMatch(code, "([A-Z][a-z]{3})?"))
    return true;

  if (optional_error_message)
    *optional_error_message =
        "'" + code +
        "' is not a valid ISO 15924 script code, should be "
        "an upper case letter [A-Z] followed by 3 lower "
        "case letters [a-z]";

  return false;
}

bool PaymentsValidators::isValidShippingAddress(
    const mojom::PaymentAddressPtr& address,
    std::string* optional_error_message) {
  if (!isValidCountryCodeFormat(address->country, optional_error_message))
    return false;

  if (!isValidLanguageCodeFormat(address->language_code,
                                 optional_error_message))
    return false;

  if (!isValidScriptCodeFormat(address->script_code, optional_error_message))
    return false;

  if (address->language_code.empty() && !address->script_code.empty()) {
    if (optional_error_message)
      *optional_error_message =
          "If language code is empty, then script code should also be empty";

    return false;
  }

  return true;
}

bool PaymentsValidators::isValidErrorMsgFormat(
    const std::string& error,
    std::string* optional_error_message) {
  if (error.length() <= maximumStringLength)
    return true;

  if (optional_error_message)
    *optional_error_message =
        "Error message should be at most 2048 characters long";

  return false;
}

}  // namespace payments
