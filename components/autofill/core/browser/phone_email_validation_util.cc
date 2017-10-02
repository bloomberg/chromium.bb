// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/phone_email_validation_util.h"

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_data.h"
#include "components/autofill/core/browser/validation.h"
#include "third_party/libphonenumber/dist/cpp/src/phonenumbers/phonenumberutil.h"

namespace autofill {

namespace {
using ::i18n::phonenumbers::PhoneNumberUtil;
}  // namespace

namespace phone_email_validation_util {

AutofillProfile::ValidityState ValidateEmailAddress(
    const base::string16& email) {
  if (email.empty())
    return AutofillProfile::EMPTY;

  return (autofill::IsValidEmailAddress(email) ? AutofillProfile::VALID
                                               : AutofillProfile::INVALID);
}

AutofillProfile::ValidityState ValidatePhoneNumber(
    const std::string& phone_number,
    const std::string& country_code) {
  if (phone_number.empty())
    return AutofillProfile::EMPTY;

  if (!base::ContainsValue(CountryDataMap::GetInstance()->country_codes(),
                           country_code)) {
    // If the country code is not in the database, the phone number cannot be
    // validated.
    return AutofillProfile::UNVALIDATED;
  }
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  return phone_util->IsPossibleNumberForString(phone_number, country_code)
             ? AutofillProfile::VALID
             : AutofillProfile::INVALID;
}

AutofillProfile::ValidityState ValidatePhoneAndEmail(AutofillProfile* profile) {
  DCHECK(profile);

  AutofillProfile::ValidityState phone_validity = ValidatePhoneNumber(
      base::UTF16ToUTF8(profile->GetRawInfo(PHONE_HOME_WHOLE_NUMBER)),
      base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY)));
  profile->SetValidityState(PHONE_HOME_WHOLE_NUMBER, phone_validity);

  AutofillProfile::ValidityState email_validity =
      ValidateEmailAddress(profile->GetRawInfo(EMAIL_ADDRESS));
  profile->SetValidityState(EMAIL_ADDRESS, email_validity);

  if (phone_validity == AutofillProfile::VALID &&
      email_validity == AutofillProfile::VALID) {
    return AutofillProfile::VALID;
  }
  return AutofillProfile::INVALID;
}

}  // namespace phone_email_validation_util
}  // namespace autofill
