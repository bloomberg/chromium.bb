// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/phone_validation_util.h"

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_data.h"
#include "third_party/libphonenumber/dist/cpp/src/phonenumbers/phonenumberutil.h"

namespace autofill {

namespace {
using ::i18n::phonenumbers::PhoneNumberUtil;
}  // namespace

namespace phone_validation_util {

AutofillProfile::ValidityState ValidatePhoneNumber(AutofillProfile* profile) {
  if (!profile)
    return AutofillProfile::UNVALIDATED;

  AutofillProfile::ValidityState validity_state;
  std::string phone_number =
      base::UTF16ToUTF8(profile->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  std::string country_code =
      base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY));

  if (phone_number.empty()) {
    // Every profile needs a phone number.
    validity_state = AutofillProfile::INVALID;
  } else if (!base::ContainsValue(
                 CountryDataMap::GetInstance()->country_codes(),
                 country_code)) {
    // If the country code is not in the database, the phone number cannot be
    // validated.
    return AutofillProfile::UNVALIDATED;
  } else {
    PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
    validity_state =
        phone_util->IsPossibleNumberForString(phone_number, country_code)
            ? AutofillProfile::VALID
            : AutofillProfile::INVALID;
  }
  profile->SetValidityState(PHONE_HOME_WHOLE_NUMBER, validity_state);

  return validity_state;
}

}  // namespace phone_validation_util
}  // namespace autofill
