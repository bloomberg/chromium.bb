// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_data_util.h"

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_method_data.h"
#include "third_party/libphonenumber/phonenumber_api.h"

namespace payments {
namespace data_util {

namespace {
using ::i18n::phonenumbers::PhoneNumber;
using ::i18n::phonenumbers::PhoneNumberUtil;

// Formats the |phone_number| to the specified |format|. Returns the original
// number if the operation is not possible.
std::string FormatPhoneNumber(const std::string& phone_number,
                              const std::string& country_code,
                              PhoneNumberUtil::PhoneNumberFormat format) {
  PhoneNumber parsed_number;
  PhoneNumberUtil* phone_number_util = PhoneNumberUtil::GetInstance();
  if (phone_number_util->Parse(phone_number, country_code, &parsed_number) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return phone_number;
  }

  std::string formatted_number;
  phone_number_util->Format(parsed_number, format, &formatted_number);
  return formatted_number;
}

}  // namespace

PaymentAddress GetPaymentAddressFromAutofillProfile(
    const autofill::AutofillProfile& profile,
    const std::string& app_locale) {
  PaymentAddress address;
  address.country = profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY);
  address.address_line = base::SplitString(
      profile.GetInfo(
          autofill::AutofillType(autofill::ADDRESS_HOME_STREET_ADDRESS),
          app_locale),
      base::ASCIIToUTF16("\n"), base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  address.region = profile.GetRawInfo(autofill::ADDRESS_HOME_STATE);
  address.city = profile.GetRawInfo(autofill::ADDRESS_HOME_CITY);
  address.dependent_locality =
      profile.GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY);
  address.postal_code = profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP);
  address.sorting_code =
      profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE);
  address.language_code = base::UTF8ToUTF16(profile.language_code());
  address.organization = profile.GetRawInfo(autofill::COMPANY_NAME);
  address.recipient =
      profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL), app_locale);
  address.phone = profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER);

  return address;
}

BasicCardResponse GetBasicCardResponseFromAutofillCreditCard(
    const autofill::CreditCard& card,
    const base::string16& cvc,
    const autofill::AutofillProfile& billing_profile,
    const std::string& app_locale) {
  BasicCardResponse response;
  response.cardholder_name = card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL);
  response.card_number = card.GetRawInfo(autofill::CREDIT_CARD_NUMBER);
  response.expiry_month = card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH);
  response.expiry_year =
      card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  response.card_security_code = cvc;

  response.billing_address =
      GetPaymentAddressFromAutofillProfile(billing_profile, app_locale);

  return response;
}

void ParseBasicCardSupportedNetworks(
    const std::vector<PaymentMethodData>& method_data,
    std::vector<std::string>* out_supported_networks,
    std::set<std::string>* out_basic_card_specified_networks) {
  DCHECK(out_supported_networks->empty());
  DCHECK(out_basic_card_specified_networks->empty());

  const std::set<std::string> kBasicCardNetworks{
      "amex",       "diners", "discover", "jcb",
      "mastercard", "mir",    "unionpay", "visa"};
  std::set<std::string> remaining_card_networks(kBasicCardNetworks);
  for (const PaymentMethodData& method_data_entry : method_data) {
    if (method_data_entry.supported_methods.empty())
      return;

    for (const std::string& method : method_data_entry.supported_methods) {
      if (method.empty())
        continue;

      const char kBasicCardMethodName[] = "basic-card";
      // If a card network is specified right in "supportedMethods", add it.
      auto card_it = remaining_card_networks.find(method);
      if (card_it != remaining_card_networks.end()) {
        out_supported_networks->push_back(method);
        // |method| removed from |remaining_card_networks| so that it is not
        // doubly added to |out_supported_networks|.
        remaining_card_networks.erase(card_it);
      } else if (method == kBasicCardMethodName) {
        // For the "basic-card" method, check "supportedNetworks".
        if (method_data_entry.supported_networks.empty()) {
          // Empty |supported_networks| means all networks are supported.
          out_supported_networks->insert(out_supported_networks->end(),
                                         remaining_card_networks.begin(),
                                         remaining_card_networks.end());
          out_basic_card_specified_networks->insert(kBasicCardNetworks.begin(),
                                                    kBasicCardNetworks.end());
          // Clear the set so that no further networks are added to
          // |out_supported_networks|.
          remaining_card_networks.clear();
        } else {
          // The merchant has specified a few basic card supported networks. Use
          // the mapping to transform to known basic-card types.
          for (const std::string& supported_network :
               method_data_entry.supported_networks) {
            // Make sure that the network was not already added to
            // |out_supported_networks|. If it's still in
            // |remaining_card_networks| it's fair game.
            auto it = remaining_card_networks.find(supported_network);
            if (it != remaining_card_networks.end()) {
              out_supported_networks->push_back(supported_network);
              remaining_card_networks.erase(it);
            }
            if (kBasicCardNetworks.find(supported_network) !=
                kBasicCardNetworks.end()) {
              out_basic_card_specified_networks->insert(supported_network);
            }
          }
        }
      }
    }
  }
}

base::string16 GetFormattedPhoneNumberForDisplay(
    const autofill::AutofillProfile& profile,
    const std::string& locale) {
  // Since the "+" is removed for some country's phone numbers, try to add a "+"
  // and see if it is a valid phone number for a country.
  // Having two "+" in front of a number has no effect on the formatted number.
  // The reason for this is international phone numbers for another country. For
  // example, without adding a "+", the US number 1-415-123-1234 for an AU
  // address would be wrongly formatted as +61 1-415-123-1234 which is invalid.
  std::string phone = base::UTF16ToUTF8(profile.GetInfo(
      autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER), locale));
  std::string tentative_intl_phone = "+" + phone;

  // Always favor the tentative international phone number if it's determined as
  // being a valid number.
  if (autofill::IsValidPhoneNumber(
          base::UTF8ToUTF16(tentative_intl_phone),
          GetCountryCodeWithFallback(&profile, locale))) {
    return base::UTF8ToUTF16(FormatPhoneForDisplay(
        tentative_intl_phone, GetCountryCodeWithFallback(&profile, locale)));
  }

  return base::UTF8ToUTF16(FormatPhoneForDisplay(
      phone, GetCountryCodeWithFallback(&profile, locale)));
}

std::string FormatPhoneForDisplay(const std::string& phone_number,
                                  const std::string& country_code) {
  return FormatPhoneNumber(phone_number, country_code,
                           PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL);
}

std::string FormatPhoneForResponse(const std::string& phone_number,
                                   const std::string& country_code) {
  return FormatPhoneNumber(phone_number, country_code,
                           PhoneNumberUtil::PhoneNumberFormat::E164);
}

base::string16 FormatCardNumberForDisplay(const base::string16& card_number) {
  base::string16 number = autofill::CreditCard::StripSeparators(card_number);
  if (number.empty() || !base::IsAsciiDigit(number[0]))
    return card_number;

  std::vector<size_t> positions = {4U, 9U, 14U};
  if (autofill::CreditCard::GetCardNetwork(number) ==
      autofill::kAmericanExpressCard) {
    positions = {4U, 11U};
  }

  static const base::char16 kSeparator = base::ASCIIToUTF16(" ")[0];
  for (size_t i : positions) {
    if (number.size() > i)
      number.insert(i, 1U, kSeparator);
  }

  return number;
}

std::string GetCountryCodeWithFallback(const autofill::AutofillProfile* profile,
                                       const std::string& app_locale) {
  std::string country_code =
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (!autofill::data_util::IsValidCountryCode(country_code))
    country_code = autofill::AutofillCountry::CountryCodeForLocale(app_locale);
  return country_code;
}

}  // namespace data_util
}  // namespace payments
