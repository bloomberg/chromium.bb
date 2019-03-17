// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter_utils.h"

#include <memory>

#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"

namespace autofill {

bool ContainsName(uint32_t groups) {
  return groups & label_formatter_groups::kName;
}

bool ContainsAddress(uint32_t groups) {
  return groups & label_formatter_groups::kAddress;
}

bool ContainsEmail(uint32_t groups) {
  return groups & label_formatter_groups::kEmail;
}

bool ContainsPhone(uint32_t groups) {
  return groups & label_formatter_groups::kPhone;
}

uint32_t DetermineGroups(const std::vector<ServerFieldType>& field_types) {
  uint32_t group_bitmask = 0;
  for (const ServerFieldType& type : field_types) {
    const FieldTypeGroup group =
        AutofillType(AutofillType(type).GetStorableType()).group();
    switch (group) {
      case autofill::NAME:
        group_bitmask |= label_formatter_groups::kName;
        break;
      case autofill::ADDRESS_HOME:
        group_bitmask |= label_formatter_groups::kAddress;
        break;
      case autofill::EMAIL:
        group_bitmask |= label_formatter_groups::kEmail;
        break;
      case autofill::PHONE_HOME:
        group_bitmask |= label_formatter_groups::kPhone;
        break;
      default:
        break;
    }
  }
  return group_bitmask;
}

AutofillProfile MakeTrimmedProfile(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& field_types) {
  AutofillProfile trimmed_profile(profile.guid(), profile.origin());
  trimmed_profile.set_language_code(profile.language_code());

  const AutofillType country_code_type(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const base::string16 country_code =
      profile.GetInfo(country_code_type, app_locale);
  trimmed_profile.SetInfo(country_code_type, country_code, app_locale);

  for (const ServerFieldType& type : field_types) {
    trimmed_profile.SetInfo(type, profile.GetInfo(type, app_locale),
                            app_locale);
  }
  return trimmed_profile;
}

base::string16 GetLabelName(const AutofillProfile& profile,
                            const std::string& app_locale) {
  return profile.GetInfo(AutofillType(NAME_FULL), app_locale);
}

base::string16 GetLabelNationalAddress(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& field_types) {
  std::unique_ptr<::i18n::addressinput::AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(
          MakeTrimmedProfile(profile, app_locale, field_types), app_locale);

  std::string address_line;
  ::i18n::addressinput::GetFormattedNationalAddressLine(*address_data,
                                                        &address_line);
  return base::UTF8ToUTF16(address_line);
}

base::string16 GetLabelEmail(const AutofillProfile& profile,
                             const std::string& app_locale) {
  return profile.GetInfo(AutofillType(EMAIL_ADDRESS), app_locale);
}

base::string16 GetLabelPhone(const AutofillProfile& profile,
                             const std::string& app_locale) {
  const std::string unformatted_phone = base::UTF16ToUTF8(
      profile.GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), app_locale));
  return unformatted_phone.empty()
             ? base::string16()
             : base::UTF8ToUTF16(i18n::FormatPhoneNationallyForDisplay(
                   unformatted_phone,
                   data_util::GetCountryCodeWithFallback(profile, app_locale)));
}

}  // namespace autofill
