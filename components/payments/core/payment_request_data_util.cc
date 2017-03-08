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

namespace payments {
namespace data_util {

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

}  // namespace data_util
}  // namespace payments
