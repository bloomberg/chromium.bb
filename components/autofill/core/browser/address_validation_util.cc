// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_validation_util.h"

#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/country_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_validator.h"

namespace autofill {

namespace {

using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::STREET_ADDRESS;
using ::i18n::addressinput::RECIPIENT;

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressProblem;
using ::i18n::addressinput::FieldProblemMap;

using ::i18n::addressinput::INVALID_FORMAT;
using ::i18n::addressinput::MISMATCHING_VALUE;
using ::i18n::addressinput::MISSING_REQUIRED_FIELD;
using ::i18n::addressinput::UNEXPECTED_FIELD;
using ::i18n::addressinput::UNKNOWN_VALUE;

const AddressField kFields[] = {COUNTRY, ADMIN_AREA, LOCALITY,
                                DEPENDENT_LOCALITY, POSTAL_CODE};
const AddressProblem kProblems[] = {UNEXPECTED_FIELD, MISSING_REQUIRED_FIELD,
                                    UNKNOWN_VALUE, INVALID_FORMAT,
                                    MISMATCHING_VALUE};

// If the |address_field| is valid, set the validity state of the
// |address_field| in the |profile| to the |state| and return true.
// Otherwise, return false.
bool SetValidityStateForAddressField(AutofillProfile* profile,
                                     AddressField address_field,
                                     AutofillProfile::ValidityState state) {
  ServerFieldType server_field = i18n::TypeForField(address_field,
                                                    /*billing=*/false);
  if (server_field == UNKNOWN_TYPE)
    return false;
  DCHECK(profile);
  profile->SetValidityState(server_field, state);
  return true;
}

// Set the validity state of all address fields in the |profile| to |state|.
void SetAllValidityStates(AutofillProfile* profile,
                          AutofillProfile::ValidityState state) {
  DCHECK(profile);
  for (auto field : kFields)
    SetValidityStateForAddressField(profile, field, state);
}

// Returns all relevant pairs of (field, problem), where field is in
// |kFields|, and problem is in |kProblems|.
FieldProblemMap* CreateFieldProblemMap() {
  FieldProblemMap* filter = new FieldProblemMap();
  for (auto field : kFields) {
    for (auto problem : kProblems) {
      filter->insert(std::make_pair(field, problem));
    }
  }
  return filter;
}

// GetFilter() will make sure that the validation only returns problems that
// are relevant.
const FieldProblemMap* GetFilter() {
  static const FieldProblemMap* const filter = CreateFieldProblemMap();
  return filter;
}

// Initializes |address| data from the address info in the |profile|.
void InitializeAddressFromProfile(const AutofillProfile& profile,
                                  AddressData* address) {
  address->region_code =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  address->administrative_area =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STATE));
  address->locality = base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_CITY));
  address->dependent_locality =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
  // The validation is case insensitive, and the postal codes are always upper
  // case.
  address->postal_code = base::UTF16ToUTF8(
      base::i18n::ToUpper(profile.GetRawInfo(ADDRESS_HOME_ZIP)));
}

void SetEmptyValidityIfEmpty(AutofillProfile* profile) {
  if (profile->GetRawInfo(ADDRESS_HOME_COUNTRY).empty())
    profile->SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::EMPTY);
  if (profile->GetRawInfo(ADDRESS_HOME_STATE).empty())
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::EMPTY);
  if (profile->GetRawInfo(ADDRESS_HOME_CITY).empty())
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::EMPTY);
  if (profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY).empty())
    profile->SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                              AutofillProfile::EMPTY);
  if (profile->GetRawInfo(ADDRESS_HOME_ZIP).empty())
    profile->SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::EMPTY);
}

}  // namespace

namespace address_validation_util {

AutofillProfile::ValidityState ValidateAddress(
    AutofillProfile* profile,
    AddressValidator* address_validator) {
  DCHECK(address_validator);
  if (!profile)
    return AutofillProfile::UNVALIDATED;

  if (!base::ContainsValue(
          CountryDataMap::GetInstance()->country_codes(),
          base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY)))) {
    // If the country code is not in the database, the country code and the
    // profile are invalid, and other fields cannot be validated, because it is
    // unclear which, if any, rule should apply.
    SetAllValidityStates(profile, AutofillProfile::UNVALIDATED);
    SetValidityStateForAddressField(profile, COUNTRY, AutofillProfile::INVALID);
    SetEmptyValidityIfEmpty(profile);
    return AutofillProfile::INVALID;
  }

  AddressData address;
  InitializeAddressFromProfile(*profile, &address);

  AutofillProfile::ValidityState profile_validity;
  FieldProblemMap problems;
  // status denotes if the rule was successfully loaded before validation.
  AddressValidator::Status status =
      address_validator->ValidateAddress(address, GetFilter(), &problems);

  if (status == AddressValidator::SUCCESS) {
    // The rules were found and applied. Initialize all fields to VALID here and
    // update the fields with problems below.
    profile_validity = AutofillProfile::VALID;
    SetAllValidityStates(profile, AutofillProfile::VALID);
  } else {
    // If the rules are not yet available, ValidateAddress can still check for
    // MISSING_REQUIRED_FIELD. In this case, the address fields will be either
    // UNVALIDATED or INVALID.
    profile_validity = AutofillProfile::UNVALIDATED;
    SetAllValidityStates(profile, AutofillProfile::UNVALIDATED);
    SetValidityStateForAddressField(profile, COUNTRY, AutofillProfile::VALID);
  }

  for (auto problem : problems) {
    if (SetValidityStateForAddressField(profile, problem.first,
                                        AutofillProfile::INVALID)) {
      profile_validity = AutofillProfile::INVALID;
    }
  }

  SetEmptyValidityIfEmpty(profile);

  return profile_validity;
}

}  // namespace address_validation_util
}  // namespace autofill
