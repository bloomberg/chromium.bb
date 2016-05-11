// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>

#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/contact_info.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/state_names.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "grit/components_strings.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using i18n::addressinput::AddressData;
using i18n::addressinput::AddressField;

namespace autofill {
namespace {

// Like |AutofillType::GetStorableType()|, but also returns |NAME_FULL| for
// first, middle, and last name field types, and groups phone number types
// similarly.
ServerFieldType GetStorableTypeCollapsingGroups(ServerFieldType type) {
  ServerFieldType storable_type = AutofillType(type).GetStorableType();
  if (AutofillType(storable_type).group() == NAME)
    return NAME_FULL;

  if (AutofillType(storable_type).group() == PHONE_HOME)
    return PHONE_HOME_WHOLE_NUMBER;

  return storable_type;
}

// Returns a value that represents specificity/privacy of the given type. This
// is used for prioritizing which data types are shown in inferred labels. For
// example, if the profile is going to fill ADDRESS_HOME_ZIP, it should
// prioritize showing that over ADDRESS_HOME_STATE in the suggestion sublabel.
int SpecificityForType(ServerFieldType type) {
  switch (type) {
    case ADDRESS_HOME_LINE1:
      return 1;

    case ADDRESS_HOME_LINE2:
      return 2;

    case EMAIL_ADDRESS:
      return 3;

    case PHONE_HOME_WHOLE_NUMBER:
      return 4;

    case NAME_FULL:
      return 5;

    case ADDRESS_HOME_ZIP:
      return 6;

    case ADDRESS_HOME_SORTING_CODE:
      return 7;

    case COMPANY_NAME:
      return 8;

    case ADDRESS_HOME_CITY:
      return 9;

    case ADDRESS_HOME_STATE:
      return 10;

    case ADDRESS_HOME_COUNTRY:
      return 11;

    default:
      break;
  }

  // The priority of other types is arbitrary, but deterministic.
  return 100 + type;
}

bool CompareSpecificity(ServerFieldType type1, ServerFieldType type2) {
  return SpecificityForType(type1) < SpecificityForType(type2);
}

// Fills |distinguishing_fields| with a list of fields to use when creating
// labels that can help to distinguish between two profiles. Draws fields from
// |suggested_fields| if it is non-NULL; otherwise returns a default list.
// If |suggested_fields| is non-NULL, does not include |excluded_field| in the
// list. Otherwise, |excluded_field| is ignored, and should be set to
// |UNKNOWN_TYPE| by convention. The resulting list of fields is sorted in
// decreasing order of importance.
void GetFieldsForDistinguishingProfiles(
    const std::vector<ServerFieldType>* suggested_fields,
    ServerFieldType excluded_field,
    std::vector<ServerFieldType>* distinguishing_fields) {
  static const ServerFieldType kDefaultDistinguishingFields[] = {
    NAME_FULL,
    ADDRESS_HOME_LINE1,
    ADDRESS_HOME_LINE2,
    ADDRESS_HOME_DEPENDENT_LOCALITY,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ZIP,
    ADDRESS_HOME_SORTING_CODE,
    ADDRESS_HOME_COUNTRY,
    EMAIL_ADDRESS,
    PHONE_HOME_WHOLE_NUMBER,
    COMPANY_NAME,
  };

  std::vector<ServerFieldType> default_fields;
  if (!suggested_fields) {
    default_fields.assign(
        kDefaultDistinguishingFields,
        kDefaultDistinguishingFields + arraysize(kDefaultDistinguishingFields));
    if (excluded_field == UNKNOWN_TYPE) {
      distinguishing_fields->swap(default_fields);
      return;
    }
    suggested_fields = &default_fields;
  }

  // Keep track of which fields we've seen so that we avoid duplicate entries.
  // Always ignore fields of unknown type and the excluded field.
  std::set<ServerFieldType> seen_fields;
  seen_fields.insert(UNKNOWN_TYPE);
  seen_fields.insert(GetStorableTypeCollapsingGroups(excluded_field));

  distinguishing_fields->clear();
  for (const ServerFieldType& it : *suggested_fields) {
    ServerFieldType suggested_type = GetStorableTypeCollapsingGroups(it);
    if (seen_fields.insert(suggested_type).second)
      distinguishing_fields->push_back(suggested_type);
  }

  std::sort(distinguishing_fields->begin(), distinguishing_fields->end(),
            CompareSpecificity);

  // Special case: If the excluded field is a partial name (e.g. first name) and
  // the suggested fields include other name fields, include |NAME_FULL| in the
  // list of distinguishing fields as a last-ditch fallback. This allows us to
  // distinguish between profiles that are identical except for the name.
  ServerFieldType effective_excluded_type =
      GetStorableTypeCollapsingGroups(excluded_field);
  if (excluded_field != effective_excluded_type) {
    for (const ServerFieldType& it : *suggested_fields) {
      if (it != excluded_field &&
          GetStorableTypeCollapsingGroups(it) == effective_excluded_type) {
        distinguishing_fields->push_back(effective_excluded_type);
        break;
      }
    }
  }
}

// Collapse compound field types to their "full" type.  I.e. First name
// collapses to full name, area code collapses to full phone, etc.
void CollapseCompoundFieldTypes(ServerFieldTypeSet* type_set) {
  ServerFieldTypeSet collapsed_set;
  for (const auto& it : *type_set) {
    switch (it) {
      case NAME_FIRST:
      case NAME_MIDDLE:
      case NAME_LAST:
      case NAME_MIDDLE_INITIAL:
      case NAME_FULL:
      case NAME_SUFFIX:
        collapsed_set.insert(NAME_FULL);
        break;

      case PHONE_HOME_NUMBER:
      case PHONE_HOME_CITY_CODE:
      case PHONE_HOME_COUNTRY_CODE:
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_HOME_WHOLE_NUMBER:
        collapsed_set.insert(PHONE_HOME_WHOLE_NUMBER);
        break;

      default:
        collapsed_set.insert(it);
    }
  }
  std::swap(*type_set, collapsed_set);
}

class FindByPhone {
 public:
  FindByPhone(const base::string16& phone,
              const std::string& country_code,
              const std::string& app_locale)
      : phone_(phone),
        country_code_(country_code),
        app_locale_(app_locale) {}

  bool operator()(const base::string16& phone) {
    return i18n::PhoneNumbersMatch(phone, phone_, country_code_, app_locale_);
  }

 private:
  base::string16 phone_;
  std::string country_code_;
  std::string app_locale_;
};

}  // namespace

AutofillProfile::AutofillProfile(const std::string& guid,
                                 const std::string& origin)
    : AutofillDataModel(guid, origin),
      record_type_(LOCAL_PROFILE),
      phone_number_(this) {
}

AutofillProfile::AutofillProfile(RecordType type, const std::string& server_id)
    : AutofillDataModel(base::GenerateGUID(), std::string()),
      record_type_(type),
      phone_number_(this),
      server_id_(server_id) {
  DCHECK(type == SERVER_PROFILE);
}

AutofillProfile::AutofillProfile()
    : AutofillDataModel(base::GenerateGUID(), std::string()),
      record_type_(LOCAL_PROFILE),
      phone_number_(this) {
}

AutofillProfile::AutofillProfile(const AutofillProfile& profile)
    : AutofillDataModel(std::string(), std::string()), phone_number_(this) {
  operator=(profile);
}

AutofillProfile::~AutofillProfile() {
}

AutofillProfile& AutofillProfile::operator=(const AutofillProfile& profile) {
  set_use_count(profile.use_count());
  set_use_date(profile.use_date());
  set_modification_date(profile.modification_date());

  if (this == &profile)
    return *this;

  set_guid(profile.guid());
  set_origin(profile.origin());

  record_type_ = profile.record_type_;

  name_ = profile.name_;
  email_ = profile.email_;
  company_ = profile.company_;
  phone_number_ = profile.phone_number_;
  phone_number_.set_profile(this);

  address_ = profile.address_;
  set_language_code(profile.language_code());

  server_id_ = profile.server_id();

  return *this;
}

// TODO(crbug.com/589535): Disambiguate similar field types before uploading.
void AutofillProfile::GetMatchingTypes(
    const base::string16& text,
    const std::string& app_locale,
    ServerFieldTypeSet* matching_types) const {
  FormGroupList info = FormGroups();
  for (const auto& it : info) {
    it->GetMatchingTypes(text, app_locale, matching_types);
  }
}

base::string16 AutofillProfile::GetRawInfo(ServerFieldType type) const {
  const FormGroup* form_group = FormGroupForType(AutofillType(type));
  if (!form_group)
    return base::string16();

  return form_group->GetRawInfo(type);
}

void AutofillProfile::SetRawInfo(ServerFieldType type,
                                 const base::string16& value) {
  FormGroup* form_group = MutableFormGroupForType(AutofillType(type));
  if (form_group)
    form_group->SetRawInfo(type, value);
}

base::string16 AutofillProfile::GetInfo(const AutofillType& type,
                                        const std::string& app_locale) const {
  if (type.html_type() == HTML_TYPE_FULL_ADDRESS) {
    std::unique_ptr<AddressData> address_data =
        i18n::CreateAddressDataFromAutofillProfile(*this, app_locale);
    if (!addressinput::HasAllRequiredFields(*address_data))
      return base::string16();

    std::vector<std::string> lines;
    ::i18n::addressinput::GetFormattedNationalAddress(*address_data, &lines);
    return base::UTF8ToUTF16(base::JoinString(lines, "\n"));
  }

  const FormGroup* form_group = FormGroupForType(type);
  if (!form_group)
    return base::string16();

  return form_group->GetInfo(type, app_locale);
}

bool AutofillProfile::SetInfo(const AutofillType& type,
                              const base::string16& value,
                              const std::string& app_locale) {
  FormGroup* form_group = MutableFormGroupForType(type);
  if (!form_group)
    return false;

  base::string16 trimmed_value;
  base::TrimWhitespace(value, base::TRIM_ALL, &trimmed_value);
  return form_group->SetInfo(type, trimmed_value, app_locale);
}

bool AutofillProfile::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

bool AutofillProfile::IsPresentButInvalid(ServerFieldType type) const {
  std::string country = UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY));
  base::string16 data = GetRawInfo(type);
  if (data.empty())
    return false;

  switch (type) {
    case ADDRESS_HOME_STATE:
      return country == "US" && !IsValidState(data);

    case ADDRESS_HOME_ZIP:
      return country == "US" && !IsValidZip(data);

    case PHONE_HOME_WHOLE_NUMBER:
      return !i18n::PhoneObject(data, country).IsValidNumber();

    case EMAIL_ADDRESS:
      return !IsValidEmailAddress(data);

    default:
      NOTREACHED();
      return false;
  }
}

int AutofillProfile::Compare(const AutofillProfile& profile) const {
  const ServerFieldType types[] = {
      NAME_FULL,
      NAME_FIRST,
      NAME_MIDDLE,
      NAME_LAST,
      COMPANY_NAME,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
  };

  for (size_t i = 0; i < arraysize(types); ++i) {
    int comparison = GetRawInfo(types[i]).compare(profile.GetRawInfo(types[i]));
    if (comparison != 0) {
      return comparison;
    }
  }

  return 0;
}

bool AutofillProfile::EqualsSansOrigin(const AutofillProfile& profile) const {
  return guid() == profile.guid() &&
         language_code() == profile.language_code() &&
         Compare(profile) == 0;
}

bool AutofillProfile::EqualsForSyncPurposes(const AutofillProfile& profile)
    const {
  return use_count() == profile.use_count() &&
         use_date() == profile.use_date() &&
         EqualsSansGuid(profile);
}

bool AutofillProfile::operator==(const AutofillProfile& profile) const {
  return guid() == profile.guid() && EqualsSansGuid(profile);
}

bool AutofillProfile::operator!=(const AutofillProfile& profile) const {
  return !operator==(profile);
}

const base::string16 AutofillProfile::PrimaryValue() const {
  return GetRawInfo(NAME_FIRST) + GetRawInfo(NAME_LAST) +
         GetRawInfo(ADDRESS_HOME_LINE1) + GetRawInfo(ADDRESS_HOME_CITY);
}

bool AutofillProfile::IsSubsetOf(const AutofillProfile& profile,
                                 const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  return IsSubsetOfForFieldSet(profile, app_locale, types);
}

bool AutofillProfile::IsSubsetOfForFieldSet(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const ServerFieldTypeSet& types) const {
  std::unique_ptr<l10n::CaseInsensitiveCompare> compare;

  for (ServerFieldType type : types) {
    base::string16 value = GetRawInfo(type);
    if (value.empty())
      continue;

    if (type == NAME_FULL || type == ADDRESS_HOME_STREET_ADDRESS) {
      // Ignore the compound "full name" field type.  We are only interested in
      // comparing the constituent parts.  For example, if |this| has a middle
      // name saved, but |profile| lacks one, |profile| could still be a subset
      // of |this|.  Likewise, ignore the compound "street address" type, as we
      // are only interested in matching line-by-line.
      continue;
    } else if (AutofillType(type).group() == PHONE_HOME) {
      // Phone numbers should be canonicalized prior to being compared.
      if (type != PHONE_HOME_WHOLE_NUMBER) {
        continue;
      } else if (!i18n::PhoneNumbersMatch(
                     value, profile.GetRawInfo(type),
                     base::UTF16ToASCII(GetRawInfo(ADDRESS_HOME_COUNTRY)),
                     app_locale)) {
        return false;
      }
    } else {
      if (!compare)
        compare.reset(new l10n::CaseInsensitiveCompare());
      if (!compare->StringsEqual(value, profile.GetRawInfo(type)))
        return false;
    }
  }

  return true;
}

bool AutofillProfile::OverwriteName(const NameInfo& imported_name,
                                    const std::string& app_locale) {
  // Check if the names parts are equal.
  if (name_.ParsedNamesAreEqual(imported_name)) {
    // If the current |name_| has an empty NAME_FULL but the the |imported_name|
    // has not, overwrite only NAME_FULL.
    if (name_.GetRawInfo(NAME_FULL).empty() &&
        !imported_name.GetRawInfo(NAME_FULL).empty()) {
      name_.SetRawInfo(NAME_FULL, imported_name.GetRawInfo(NAME_FULL));
      return true;
    }
    return false;
  }

  l10n::CaseInsensitiveCompare compare;
  AutofillType type = AutofillType(NAME_FULL);
  base::string16 full_name = name_.GetInfo(type, app_locale);
  if (compare.StringsEqual(full_name,
                           imported_name.GetInfo(type, app_locale))) {
    // The imported name has the same full name string as the name for this
    // profile.  Because full names are _heuristically_ parsed into
    // {first, middle, last} name components, it's possible that either the
    // existing name or the imported name was misparsed.  Prefer to keep the
    // name whose {first, middle, last} components do not match those computed
    // by the heuristic parse, as this more likely represents the correct,
    // user-input parse of the name.
    NameInfo heuristically_parsed_name;
    heuristically_parsed_name.SetInfo(type, full_name, app_locale);
    if (imported_name.ParsedNamesAreEqual(heuristically_parsed_name))
      return false;
  }

  name_ = imported_name;
  return true;
}

bool AutofillProfile::OverwriteWith(const AutofillProfile& profile,
                                    const std::string& app_locale) {
  // Verified profiles should never be overwritten with unverified data.
  DCHECK(!IsVerified() || profile.IsVerified());
  set_origin(profile.origin());
  set_language_code(profile.language_code());
  set_use_count(profile.use_count() + use_count());
  if (profile.use_date() > use_date())
    set_use_date(profile.use_date());

  // |types_to_overwrite| is initially populated with all types that have
  //  non-empty data in the incoming |profile|. After adjustment, all data from
  // |profile| corresponding to types in |types_to_overwrite| is overwritten in
  // |this| profile.
  ServerFieldTypeSet types_to_overwrite;
  profile.GetNonEmptyTypes(app_locale, &types_to_overwrite);

  // Only transfer "full" types (e.g. full name) and not fragments (e.g.
  // first name, last name).
  CollapseCompoundFieldTypes(&types_to_overwrite);

  // Remove ADDRESS_HOME_STREET_ADDRESS to ensure a merge of the address line by
  // line. See comment below.
  types_to_overwrite.erase(ADDRESS_HOME_STREET_ADDRESS);

  l10n::CaseInsensitiveCompare compare;

  // Special case for addresses. With the whole address comparison, it is now
  // necessary to make sure to keep the best address format: both lines used.
  // This is because some sites might not have an address line 2 and the
  // previous value should not be replaced with an empty string in that case.
  if (compare.StringsEqual(
          CanonicalizeProfileString(
              profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS)),
          CanonicalizeProfileString(GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))) &&
      !GetRawInfo(ADDRESS_HOME_LINE2).empty() &&
      profile.GetRawInfo(ADDRESS_HOME_LINE2).empty()) {
    types_to_overwrite.erase(ADDRESS_HOME_LINE1);
    types_to_overwrite.erase(ADDRESS_HOME_LINE2);
  }

  bool did_overwrite = false;

  for (const ServerFieldType field_type : types_to_overwrite) {
    // Special case for names.
    if (AutofillType(field_type).group() == NAME) {
      did_overwrite |= OverwriteName(profile.name_, app_locale);
      continue;
    }

    base::string16 new_value = profile.GetRawInfo(field_type);
    // Overwrite the data in |this| profile for the field type and set
    // |did_overwrite| if the previous data was different than the |new_value|.
    if (GetRawInfo(field_type) != new_value) {
      SetRawInfo(field_type, new_value);
      did_overwrite = true;
    }
  }

  return did_overwrite;
}

bool AutofillProfile::SaveAdditionalInfo(const AutofillProfile& profile,
                                         const std::string& app_locale) {
  ServerFieldTypeSet field_types, other_field_types;
  GetNonEmptyTypes(app_locale, &field_types);
  profile.GetNonEmptyTypes(app_locale, &other_field_types);
  // The address needs to be compared line by line to take into account the
  // logic for empty fields implemented in the loop.
  field_types.erase(ADDRESS_HOME_STREET_ADDRESS);
  l10n::CaseInsensitiveCompare compare;
  for (ServerFieldType field_type : field_types) {
    if (other_field_types.count(field_type)) {
      AutofillType type = AutofillType(field_type);
      // Special cases for name and phone. If the whole/full value matches, skip
      // the individual fields comparison.
      if (type.group() == NAME &&
          compare.StringsEqual(
              profile.GetInfo(AutofillType(NAME_FULL), app_locale),
              GetInfo(AutofillType(NAME_FULL), app_locale))) {
        continue;
      }
      if (type.group() == PHONE_HOME &&
          i18n::PhoneNumbersMatch(
              GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
              profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
              base::UTF16ToASCII(GetRawInfo(ADDRESS_HOME_COUNTRY)),
              app_locale)) {
        continue;
      }

      // Special case for postal codes, where postal codes with/without spaces
      // in them are considered equivalent.
      if (field_type == ADDRESS_HOME_ZIP) {
        base::string16 profile_zip;
        base::string16 current_zip;
        base::RemoveChars(profile.GetRawInfo(field_type), ASCIIToUTF16(" "),
                          &profile_zip);
        base::RemoveChars(GetRawInfo(field_type), ASCIIToUTF16(" "),
                          &current_zip);
        if (!compare.StringsEqual(profile_zip, current_zip))
          return false;
        continue;
      }

      // Special case for the address because the comparison uses canonicalized
      // values. Start by comparing the address line by line. If it fails, make
      // sure that the address as a whole is different before returning false.
      // It is possible that the user put the info from line 2 on line 1 because
      // of a certain form for example.
      if (field_type == ADDRESS_HOME_LINE1 ||
          field_type == ADDRESS_HOME_LINE2) {
        if (!compare.StringsEqual(
                CanonicalizeProfileString(profile.GetRawInfo(field_type)),
                CanonicalizeProfileString(GetRawInfo(field_type))) &&
            !compare.StringsEqual(CanonicalizeProfileString(profile.GetRawInfo(
                                      ADDRESS_HOME_STREET_ADDRESS)),
                                  CanonicalizeProfileString(GetRawInfo(
                                      ADDRESS_HOME_STREET_ADDRESS)))) {
          return false;
        }
        continue;
      }
      // Special case for the state to support abbreviations. Currently only the
      // US states are supported.
      if (field_type == ADDRESS_HOME_STATE) {
        base::string16 full;
        base::string16 abbreviation;
        state_names::GetNameAndAbbreviation(GetRawInfo(ADDRESS_HOME_STATE),
                                            &full, &abbreviation);
        if (compare.StringsEqual(profile.GetRawInfo(ADDRESS_HOME_STATE),
                                 full) ||
            compare.StringsEqual(profile.GetRawInfo(ADDRESS_HOME_STATE),
                                 abbreviation))
          continue;
      }

      // Special case for company names to support cannonicalized variations.
      if (field_type == COMPANY_NAME) {
        if (compare.StringsEqual(
                CanonicalizeProfileString(profile.GetRawInfo(field_type)),
                CanonicalizeProfileString(GetRawInfo(field_type)))) {
          continue;
        }
      }

      if (!compare.StringsEqual(profile.GetRawInfo(field_type),
                                GetRawInfo(field_type))) {
        return false;
      }
    }
  }

  if (!IsVerified() || profile.IsVerified()) {
    if (OverwriteWith(profile, app_locale)) {
      AutofillMetrics::LogProfileActionOnFormSubmitted(
          AutofillMetrics::EXISTING_PROFILE_UPDATED);
    } else {
      AutofillMetrics::LogProfileActionOnFormSubmitted(
          AutofillMetrics::EXISTING_PROFILE_USED);
    }
  }
  return true;
}

// static
bool AutofillProfile::SupportsMultiValue(ServerFieldType type) {
  FieldTypeGroup group = AutofillType(type).group();
  return group == NAME ||
         group == NAME_BILLING ||
         group == EMAIL ||
         group == PHONE_HOME ||
         group == PHONE_BILLING;
}

// static
void AutofillProfile::CreateDifferentiatingLabels(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    std::vector<base::string16>* labels) {
  const size_t kMinimalFieldsShown = 2;
  CreateInferredLabels(profiles, NULL, UNKNOWN_TYPE, kMinimalFieldsShown,
                       app_locale, labels);
  DCHECK_EQ(profiles.size(), labels->size());
}

// static
void AutofillProfile::CreateInferredLabels(
    const std::vector<AutofillProfile*>& profiles,
    const std::vector<ServerFieldType>* suggested_fields,
    ServerFieldType excluded_field,
    size_t minimal_fields_shown,
    const std::string& app_locale,
    std::vector<base::string16>* labels) {
  std::vector<ServerFieldType> fields_to_use;
  GetFieldsForDistinguishingProfiles(suggested_fields, excluded_field,
                                     &fields_to_use);

  // Construct the default label for each profile. Also construct a map that
  // associates each label with the profiles that have this label. This map is
  // then used to detect which labels need further differentiating fields.
  std::map<base::string16, std::list<size_t> > labels_to_profiles;
  for (size_t i = 0; i < profiles.size(); ++i) {
    base::string16 label =
        profiles[i]->ConstructInferredLabel(fields_to_use,
                                            minimal_fields_shown,
                                            app_locale);
    labels_to_profiles[label].push_back(i);
  }

  labels->resize(profiles.size());
  for (auto& it : labels_to_profiles) {
    if (it.second.size() == 1) {
      // This label is unique, so use it without any further ado.
      base::string16 label = it.first;
      size_t profile_index = it.second.front();
      (*labels)[profile_index] = label;
    } else {
      // We have more than one profile with the same label, so add
      // differentiating fields.
      CreateInferredLabelsHelper(profiles, it.second, fields_to_use,
                                 minimal_fields_shown, app_locale, labels);
    }
  }
}

void AutofillProfile::GenerateServerProfileIdentifier() {
  DCHECK_EQ(SERVER_PROFILE, record_type());
  base::string16 contents = GetRawInfo(NAME_FIRST);
  contents.append(GetRawInfo(NAME_MIDDLE));
  contents.append(GetRawInfo(NAME_LAST));
  contents.append(GetRawInfo(EMAIL_ADDRESS));
  contents.append(GetRawInfo(COMPANY_NAME));
  contents.append(GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
  contents.append(GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
  contents.append(GetRawInfo(ADDRESS_HOME_CITY));
  contents.append(GetRawInfo(ADDRESS_HOME_STATE));
  contents.append(GetRawInfo(ADDRESS_HOME_ZIP));
  contents.append(GetRawInfo(ADDRESS_HOME_SORTING_CODE));
  contents.append(GetRawInfo(ADDRESS_HOME_COUNTRY));
  contents.append(GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  std::string contents_utf8 = UTF16ToUTF8(contents);
  contents_utf8.append(language_code());
  server_id_ = base::SHA1HashString(contents_utf8);
}

void AutofillProfile::RecordAndLogUse() {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.Profile",
                            (base::Time::Now() - use_date()).InDays());
  RecordUse();
}

// static
base::string16 AutofillProfile::CanonicalizeProfileString(
    const base::string16& str) {
  base::string16 ret;
  ret.reserve(str.size());

  bool previous_was_whitespace = false;

  // This algorithm isn't designed to be perfect, we could get arbitrarily
  // fancy here trying to canonicalize address lines. Instead, this is designed
  // to handle common cases for all types of data (addresses and names)
  // without the need of domain-specific logic.
  base::i18n::UTF16CharIterator iter(&str);
  while (!iter.end()) {
    switch (u_charType(iter.get())) {
      case U_DASH_PUNCTUATION:
      case U_START_PUNCTUATION:
      case U_END_PUNCTUATION:
      case U_CONNECTOR_PUNCTUATION:
      case U_OTHER_PUNCTUATION:
        // Convert punctuation to spaces. This will convert "Mid-Island Plz."
        // -> "Mid Island Plz" (the trailing space will be trimmed off at the
        // end of the loop).
        if (!previous_was_whitespace) {
          ret.push_back(' ');
          previous_was_whitespace = true;
        }
        break;

      case U_CONTROL_CHAR:  // To escape the '\n' character.
      case U_SPACE_SEPARATOR:
      case U_LINE_SEPARATOR:
      case U_PARAGRAPH_SEPARATOR:
        // Convert sequences of spaces to single spaces.
        if (!previous_was_whitespace) {
          ret.push_back(' ');
          previous_was_whitespace = true;
        }
        break;

      case U_UPPERCASE_LETTER:
      case U_TITLECASE_LETTER:
        previous_was_whitespace = false;
        base::WriteUnicodeCharacter(u_tolower(iter.get()), &ret);
        break;

      default:
        previous_was_whitespace = false;
        base::WriteUnicodeCharacter(iter.get(), &ret);
        break;
    }
    iter.Advance();
  }

  // Trim off trailing whitespace if we left one.
  if (previous_was_whitespace)
    ret.resize(ret.size() - 1);

  return ret;
}

// static
bool AutofillProfile::AreProfileStringsSimilar(const base::string16& a,
                                               const base::string16& b) {
  return CanonicalizeProfileString(a) == CanonicalizeProfileString(b);
}

void AutofillProfile::GetSupportedTypes(
    ServerFieldTypeSet* supported_types) const {
  FormGroupList info = FormGroups();
  for (const auto& it : info) {
    it->GetSupportedTypes(supported_types);
  }
}

base::string16 AutofillProfile::ConstructInferredLabel(
    const std::vector<ServerFieldType>& included_fields,
    size_t num_fields_to_use,
    const std::string& app_locale) const {
  // TODO(estade): use libaddressinput?
  base::string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);

  AutofillType region_code_type(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const base::string16& profile_region_code =
      GetInfo(region_code_type, app_locale);
  std::string address_region_code = UTF16ToUTF8(profile_region_code);

  // A copy of |this| pruned down to contain only data for the address fields in
  // |included_fields|.
  AutofillProfile trimmed_profile(guid(), origin());
  trimmed_profile.SetInfo(region_code_type, profile_region_code, app_locale);
  trimmed_profile.set_language_code(language_code());

  std::vector<ServerFieldType> remaining_fields;
  for (std::vector<ServerFieldType>::const_iterator it =
           included_fields.begin();
       it != included_fields.end() && num_fields_to_use > 0;
       ++it) {
    AddressField address_field;
    if (!i18n::FieldForType(*it, &address_field) ||
        !::i18n::addressinput::IsFieldUsed(
            address_field, address_region_code) ||
        address_field == ::i18n::addressinput::COUNTRY) {
      remaining_fields.push_back(*it);
      continue;
    }

    AutofillType autofill_type(*it);
    base::string16 field_value = GetInfo(autofill_type, app_locale);
    if (field_value.empty())
      continue;

    trimmed_profile.SetInfo(autofill_type, field_value, app_locale);
    --num_fields_to_use;
  }

  std::unique_ptr<AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(trimmed_profile, app_locale);
  std::string address_line;
  ::i18n::addressinput::GetFormattedNationalAddressLine(
      *address_data, &address_line);
  base::string16 label = base::UTF8ToUTF16(address_line);

  for (std::vector<ServerFieldType>::const_iterator it =
           remaining_fields.begin();
       it != remaining_fields.end() && num_fields_to_use > 0;
       ++it) {
    base::string16 field_value;
    // Special case whole numbers: we want the user-formatted (raw) version, not
    // the canonicalized version we'll fill into the page.
    if (*it == PHONE_HOME_WHOLE_NUMBER)
      field_value = GetRawInfo(*it);
    else
      field_value = GetInfo(AutofillType(*it), app_locale);
    if (field_value.empty())
      continue;

    if (!label.empty())
      label.append(separator);

    label.append(field_value);
    --num_fields_to_use;
  }

  // If country code is missing, libaddressinput won't be used to format the
  // address. In this case the suggestion might include a multi-line street
  // address which needs to be flattened.
  base::ReplaceChars(label, base::ASCIIToUTF16("\n"), separator, &label);

  return label;
}

// static
void AutofillProfile::CreateInferredLabelsHelper(
    const std::vector<AutofillProfile*>& profiles,
    const std::list<size_t>& indices,
    const std::vector<ServerFieldType>& fields,
    size_t num_fields_to_include,
    const std::string& app_locale,
    std::vector<base::string16>* labels) {
  // For efficiency, we first construct a map of fields to their text values and
  // each value's frequency.
  std::map<ServerFieldType,
           std::map<base::string16, size_t> > field_text_frequencies_by_field;
  for (const ServerFieldType& field : fields) {
    std::map<base::string16, size_t>& field_text_frequencies =
        field_text_frequencies_by_field[field];

    for (const auto& it : indices) {
      const AutofillProfile* profile = profiles[it];
      base::string16 field_text =
          profile->GetInfo(AutofillType(field), app_locale);

      // If this label is not already in the map, add it with frequency 0.
      if (!field_text_frequencies.count(field_text))
        field_text_frequencies[field_text] = 0;

      // Now, increment the frequency for this label.
      ++field_text_frequencies[field_text];
    }
  }

  // Now comes the meat of the algorithm. For each profile, we scan the list of
  // fields to use, looking for two things:
  //  1. A (non-empty) field that differentiates the profile from all others
  //  2. At least |num_fields_to_include| non-empty fields
  // Before we've satisfied condition (2), we include all fields, even ones that
  // are identical across all the profiles. Once we've satisfied condition (2),
  // we only include fields that that have at last two distinct values.
  for (const auto& it : indices) {
    const AutofillProfile* profile = profiles[it];

    std::vector<ServerFieldType> label_fields;
    bool found_differentiating_field = false;
    for (std::vector<ServerFieldType>::const_iterator field = fields.begin();
         field != fields.end(); ++field) {
      // Skip over empty fields.
      base::string16 field_text =
          profile->GetInfo(AutofillType(*field), app_locale);
      if (field_text.empty())
        continue;

      std::map<base::string16, size_t>& field_text_frequencies =
          field_text_frequencies_by_field[*field];
      found_differentiating_field |=
          !field_text_frequencies.count(base::string16()) &&
          (field_text_frequencies[field_text] == 1);

      // Once we've found enough non-empty fields, skip over any remaining
      // fields that are identical across all the profiles.
      if (label_fields.size() >= num_fields_to_include &&
          (field_text_frequencies.size() == 1))
        continue;

      label_fields.push_back(*field);

      // If we've (1) found a differentiating field and (2) found at least
      // |num_fields_to_include| non-empty fields, we're done!
      if (found_differentiating_field &&
          label_fields.size() >= num_fields_to_include)
        break;
    }

    (*labels)[it] = profile->ConstructInferredLabel(
        label_fields, label_fields.size(), app_locale);
  }
}

AutofillProfile::FormGroupList AutofillProfile::FormGroups() const {
  FormGroupList v(5);
  v[0] = &name_;
  v[1] = &email_;
  v[2] = &company_;
  v[3] = &phone_number_;
  v[4] = &address_;
  return v;
}

const FormGroup* AutofillProfile::FormGroupForType(
    const AutofillType& type) const {
  return const_cast<AutofillProfile*>(this)->MutableFormGroupForType(type);
}

FormGroup* AutofillProfile::MutableFormGroupForType(const AutofillType& type) {
  switch (type.group()) {
    case NAME:
    case NAME_BILLING:
      return &name_;

    case EMAIL:
      return &email_;

    case COMPANY:
      return &company_;

    case PHONE_HOME:
    case PHONE_BILLING:
      return &phone_number_;

    case ADDRESS_HOME:
    case ADDRESS_BILLING:
      return &address_;

    case NO_GROUP:
    case CREDIT_CARD:
    case PASSWORD_FIELD:
    case USERNAME_FIELD:
    case TRANSACTION:
        return NULL;
  }

  NOTREACHED();
  return NULL;
}

bool AutofillProfile::EqualsSansGuid(const AutofillProfile& profile) const {
  return origin() == profile.origin() &&
         language_code() == profile.language_code() &&
         Compare(profile) == 0;
}

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile) {
  return os << profile.guid() << " " << profile.origin() << " "
            << UTF16ToUTF8(profile.GetRawInfo(NAME_FIRST)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(NAME_MIDDLE)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(NAME_LAST)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(EMAIL_ADDRESS)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(COMPANY_NAME)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE1)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE2)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))
            << " " << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_CITY)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STATE)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_ZIP)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)) << " "
            << profile.language_code() << " "
            << UTF16ToUTF8(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
}

}  // namespace autofill
