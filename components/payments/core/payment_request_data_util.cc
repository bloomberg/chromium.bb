// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_data_util.h"

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
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
    const std::vector<autofill::AutofillProfile*>& billing_profiles,
    const std::string& app_locale) {
  BasicCardResponse response;
  response.cardholder_name = card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL);
  response.card_number = card.GetRawInfo(autofill::CREDIT_CARD_NUMBER);
  response.expiry_month = card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH);
  response.expiry_year =
      card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  response.card_security_code = cvc;

  // TODO(crbug.com/602666): Ensure we reach here only if the card has a billing
  // address. Then add DCHECK(!card->billing_address_id().empty()).
  if (!card.billing_address_id().empty()) {
    const autofill::AutofillProfile* billing_address =
        autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
            card.billing_address_id(), billing_profiles);
    DCHECK(billing_address);
    response.billing_address =
        GetPaymentAddressFromAutofillProfile(*billing_address, app_locale);
  }

  return response;
}

bool ParseBasicCardSupportedNetworks(
    const std::vector<PaymentMethodData>& method_data,
    std::vector<std::string>* out_supported_networks,
    std::set<std::string>* out_basic_card_specified_networks) {
  DCHECK(out_supported_networks->empty());
  DCHECK(out_basic_card_specified_networks->empty());

  std::set<std::string> card_networks{"amex",     "diners",     "discover",
                                      "jcb",      "mastercard", "mir",
                                      "unionpay", "visa"};
  for (const PaymentMethodData& method_data_entry : method_data) {
    if (method_data_entry.supported_methods.empty())
      return false;

    for (const std::string& method : method_data_entry.supported_methods) {
      if (method.empty())
        continue;

      // If a card network is specified right in "supportedMethods", add it.
      const char kBasicCardMethodName[] = "basic-card";
      auto card_it = card_networks.find(method);
      if (card_it != card_networks.end()) {
        out_supported_networks->push_back(method);
        // |method| removed from |card_networks| so that it is not doubly added
        // to |supported_card_networks_| if "basic-card" is specified with no
        // supported networks.
        card_networks.erase(card_it);
      } else if (method == kBasicCardMethodName) {
        // For the "basic-card" method, check "supportedNetworks".
        if (method_data_entry.supported_networks.empty()) {
          // Empty |supported_networks| means all networks are supported.
          out_supported_networks->insert(out_supported_networks->end(),
                                         card_networks.begin(),
                                         card_networks.end());
          out_basic_card_specified_networks->insert(card_networks.begin(),
                                                    card_networks.end());
          // Clear the set so that no further networks are added to
          // |out_supported_networks|.
          card_networks.clear();
        } else {
          // The merchant has specified a few basic card supported networks. Use
          // the mapping to transform to known basic-card types.
          for (const std::string& supported_network :
               method_data_entry.supported_networks) {
            // Make sure that the network was not already added to
            // |out_supported_networks|. If it's still in |card_networks| it's
            // fair game.
            auto it = card_networks.find(supported_network);
            if (it != card_networks.end()) {
              out_supported_networks->push_back(supported_network);
              out_basic_card_specified_networks->insert(supported_network);
              card_networks.erase(it);
            }
          }
        }
      }
    }
  }
  return true;
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

}  // namespace data_util
}  // namespace payments
