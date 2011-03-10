// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_profile.h"

#include <algorithm>
#include <set>

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/contact_info.h"
#include "chrome/browser/autofill/fax_number.h"
#include "chrome/browser/autofill/home_phone_number.h"
#include "chrome/common/guid.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Like |AutofillType::GetEquivalentFieldType()|, but also returns |NAME_FULL|
// for first, middle, and last name field types.
AutofillFieldType GetEquivalentFieldTypeCollapsingNames(
    AutofillFieldType field_type) {
  if (field_type == NAME_FIRST || field_type == NAME_MIDDLE ||
      field_type == NAME_LAST)
    return NAME_FULL;

  return AutofillType::GetEquivalentFieldType(field_type);
}

// Fills |distinguishing_fields| with a list of fields to use when creating
// labels that can help to distinguish between two profiles. Draws fields from
// |suggested_fields| if it is non-NULL; otherwise returns a default list.
// If |suggested_fields| is non-NULL, does not include |excluded_field| in the
// list. Otherwise, |excluded_field| is ignored, and should be set to
// |UNKNOWN_TYPE| by convention. The resulting list of fields is sorted in
// decreasing order of importance.
void GetFieldsForDistinguishingProfiles(
    const std::vector<AutofillFieldType>* suggested_fields,
    AutofillFieldType excluded_field,
    std::vector<AutofillFieldType>* distinguishing_fields) {
  static const AutofillFieldType kDefaultDistinguishingFields[] = {
    NAME_FULL,
    ADDRESS_HOME_LINE1,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ZIP,
    ADDRESS_HOME_COUNTRY,
    EMAIL_ADDRESS,
    PHONE_HOME_WHOLE_NUMBER,
    PHONE_FAX_WHOLE_NUMBER,
    COMPANY_NAME,
  };

  if (!suggested_fields) {
    DCHECK_EQ(excluded_field, UNKNOWN_TYPE);
    distinguishing_fields->assign(
        kDefaultDistinguishingFields,
        kDefaultDistinguishingFields + arraysize(kDefaultDistinguishingFields));
    return;
  }

  // Keep track of which fields we've seen so that we avoid duplicate entries.
  // Always ignore fields of unknown type and the excluded field.
  std::set<AutofillFieldType> seen_fields;
  seen_fields.insert(UNKNOWN_TYPE);
  seen_fields.insert(GetEquivalentFieldTypeCollapsingNames(excluded_field));

  distinguishing_fields->clear();
  for (std::vector<AutofillFieldType>::const_iterator it =
           suggested_fields->begin();
       it != suggested_fields->end(); ++it) {
    AutofillFieldType suggested_type =
        GetEquivalentFieldTypeCollapsingNames(*it);
    if (seen_fields.insert(suggested_type).second)
      distinguishing_fields->push_back(suggested_type);
  }

  // Special case: If the excluded field is a partial name (e.g. first name) and
  // the suggested fields include other name fields, include |NAME_FULL| in the
  // list of distinguishing fields as a last-ditch fallback. This allows us to
  // distinguish between profiles that are identical except for the name.
  if (excluded_field != NAME_FULL &&
      GetEquivalentFieldTypeCollapsingNames(excluded_field) == NAME_FULL) {
    for (std::vector<AutofillFieldType>::const_iterator it =
             suggested_fields->begin();
         it != suggested_fields->end(); ++it) {
      if (*it != excluded_field &&
          GetEquivalentFieldTypeCollapsingNames(*it) == NAME_FULL) {
        distinguishing_fields->push_back(NAME_FULL);
        break;
      }
    }
  }
}

}  // namespace

AutofillProfile::AutofillProfile(const std::string& guid)
    : guid_(guid) {
}

AutofillProfile::AutofillProfile()
    : guid_(guid::GenerateGUID()) {
}

AutofillProfile::AutofillProfile(const AutofillProfile& source)
    : FormGroup() {
  operator=(source);
}

AutofillProfile::~AutofillProfile() {
}

AutofillProfile& AutofillProfile::operator=(const AutofillProfile& profile) {
  if (this == &profile)
    return *this;

  label_ = profile.label_;
  guid_ = profile.guid_;

  name_ = profile.name_;
  email_ = profile.email_;
  company_ = profile.company_;
  home_number_ = profile.home_number_;
  fax_number_ = profile.fax_number_;
  address_ = profile.address_;

  return *this;
}

void AutofillProfile::GetPossibleFieldTypes(
    const string16& text,
    FieldTypeSet* possible_types) const {
  FormGroupList info = info_list();
  for (FormGroupList::const_iterator it = info.begin(); it != info.end(); ++it)
    (*it)->GetPossibleFieldTypes(text, possible_types);
}

void AutofillProfile::GetAvailableFieldTypes(
    FieldTypeSet* available_types) const {
  FormGroupList info = info_list();
  for (FormGroupList::const_iterator it = info.begin(); it != info.end(); ++it)
    (*it)->GetAvailableFieldTypes(available_types);
}

string16 AutofillProfile::GetFieldText(const AutofillType& type) const {
  AutofillType return_type(
      AutofillType::GetEquivalentFieldType(type.field_type()));

  FormGroupMap info = info_map();
  FormGroupMap::const_iterator it = info.find(return_type.group());
  if (it == info.end())
    return string16();

  return it->second->GetFieldText(return_type);
}

void AutofillProfile::FindInfoMatches(
    const AutofillType& type,
    const string16& value,
    std::vector<string16>* matched_text) const {
  if (matched_text == NULL) {
    DLOG(ERROR) << "NULL matched text passed in";
    return;
  }

  string16 clean_info = StringToLowerASCII(CollapseWhitespace(value, false));

  // If the field_type is unknown, then match against all field types.
  if (type.field_type() == UNKNOWN_TYPE) {
    FormGroupList info = info_list();
    for (FormGroupList::const_iterator it = info.begin();
         it != info.end(); ++it)
      (*it)->FindInfoMatches(type, clean_info, matched_text);
  } else {
    FormGroupMap info = info_map();
    FormGroupMap::const_iterator it = info.find(type.group());
    if (it != info.end())
      it->second->FindInfoMatches(type, clean_info, matched_text);
  }
}

void AutofillProfile::SetInfo(const AutofillType& type, const string16& value) {
  MutableFormGroupMap info = mutable_info_map();
  MutableFormGroupMap::iterator it = info.find(type.group());
  if (it != info.end())
    it->second->SetInfo(type, CollapseWhitespace(value, false));
}

const string16 AutofillProfile::Label() const {
  return label_;
}

const std::string AutofillProfile::CountryCode() const {
  return address_.country_code();
}

void AutofillProfile::SetCountryCode(const std::string& country_code) {
  address_.set_country_code(country_code);
}

// static
bool AutofillProfile::AdjustInferredLabels(
    std::vector<AutofillProfile*>* profiles) {
  const size_t kMinimalFieldsShown = 2;

  std::vector<string16> created_labels;
  CreateInferredLabels(profiles, NULL, UNKNOWN_TYPE, kMinimalFieldsShown,
                       &created_labels);
  DCHECK_EQ(profiles->size(), created_labels.size());

  bool updated_labels = false;
  for (size_t i = 0; i < profiles->size(); ++i) {
    if ((*profiles)[i]->Label() != created_labels[i]) {
      updated_labels = true;
      (*profiles)[i]->label_ = created_labels[i];
    }
  }
  return updated_labels;
}

// static
void AutofillProfile::CreateInferredLabels(
    const std::vector<AutofillProfile*>* profiles,
    const std::vector<AutofillFieldType>* suggested_fields,
    AutofillFieldType excluded_field,
    size_t minimal_fields_shown,
    std::vector<string16>* created_labels) {
  DCHECK(profiles);
  DCHECK(created_labels);

  std::vector<AutofillFieldType> fields_to_use;
  GetFieldsForDistinguishingProfiles(suggested_fields, excluded_field,
                                     &fields_to_use);

  // Construct the default label for each profile. Also construct a map that
  // associates each label with the profiles that have this label. This map is
  // then used to detect which labels need further differentiating fields.
  std::map<string16, std::list<size_t> > labels;
  for (size_t i = 0; i < profiles->size(); ++i) {
    string16 label =
        (*profiles)[i]->ConstructInferredLabel(fields_to_use,
                                               minimal_fields_shown);
    labels[label].push_back(i);
  }

  created_labels->resize(profiles->size());
  for (std::map<string16, std::list<size_t> >::const_iterator it =
           labels.begin();
       it != labels.end(); ++it) {
    if (it->second.size() == 1) {
      // This label is unique, so use it without any further ado.
      string16 label = it->first;
      size_t profile_index = it->second.front();
      (*created_labels)[profile_index] = label;
    } else {
      // We have more than one profile with the same label, so add
      // differentiating fields.
      CreateDifferentiatingLabels(*profiles, it->second, fields_to_use,
                                  minimal_fields_shown, created_labels);
    }
  }
}

bool AutofillProfile::IsEmpty() const {
  FieldTypeSet types;
  GetAvailableFieldTypes(&types);
  return types.empty();
}

int AutofillProfile::Compare(const AutofillProfile& profile) const {
  // The following AutoFill field types are the only types we store in the WebDB
  // so far, so we're only concerned with matching these types in the profile.
  const AutofillFieldType types[] = { NAME_FIRST,
                                      NAME_MIDDLE,
                                      NAME_LAST,
                                      EMAIL_ADDRESS,
                                      COMPANY_NAME,
                                      ADDRESS_HOME_LINE1,
                                      ADDRESS_HOME_LINE2,
                                      ADDRESS_HOME_CITY,
                                      ADDRESS_HOME_STATE,
                                      ADDRESS_HOME_ZIP,
                                      ADDRESS_HOME_COUNTRY,
                                      PHONE_HOME_NUMBER,
                                      PHONE_FAX_NUMBER };

  for (size_t index = 0; index < arraysize(types); ++index) {
    int comparison = GetFieldText(AutofillType(types[index])).compare(
        profile.GetFieldText(AutofillType(types[index])));
    if (comparison != 0)
      return comparison;
  }

  return 0;
}

bool AutofillProfile::operator==(const AutofillProfile& profile) const {
  return guid_ == profile.guid_ && Compare(profile) == 0;
}

bool AutofillProfile::operator!=(const AutofillProfile& profile) const {
  return !operator==(profile);
}

const string16 AutofillProfile::PrimaryValue() const {
  return GetFieldText(AutofillType(NAME_FULL)) +
         GetFieldText(AutofillType(ADDRESS_HOME_LINE1)) +
         GetFieldText(AutofillType(ADDRESS_HOME_LINE2)) +
         GetFieldText(AutofillType(EMAIL_ADDRESS));
}

string16 AutofillProfile::ConstructInferredLabel(
    const std::vector<AutofillFieldType>& included_fields,
    size_t num_fields_to_use) const {
  const string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_SUMMARY_SEPARATOR);

  string16 label;
  size_t num_fields_used = 0;
  for (std::vector<AutofillFieldType>::const_iterator it =
           included_fields.begin();
       it != included_fields.end() && num_fields_used < num_fields_to_use;
       ++it) {
    string16 field = GetFieldText(AutofillType(*it));
    if (field.empty())
      continue;

    if (!label.empty())
      label.append(separator);

    // Fax number has special format, to indicate that this is a fax number.
    if (*it == PHONE_FAX_WHOLE_NUMBER) {
      field = l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_DIALOG_ADDRESS_SUMMARY_FAX_FORMAT, field);
    }
    label.append(field);
    ++num_fields_used;
  }
  return label;
}

// static
void AutofillProfile::CreateDifferentiatingLabels(
    const std::vector<AutofillProfile*>& profiles,
    const std::list<size_t>& indices,
    const std::vector<AutofillFieldType>& fields,
    size_t num_fields_to_include,
    std::vector<string16>* created_labels) {
  // For efficiency, we first construct a map of fields to their text values and
  // each value's frequency.
  std::map<AutofillFieldType,
           std::map<string16, size_t> > field_text_frequencies_by_field;
  for (std::vector<AutofillFieldType>::const_iterator field = fields.begin();
       field != fields.end(); ++field) {
    std::map<string16, size_t>& field_text_frequencies =
        field_text_frequencies_by_field[*field];

    for (std::list<size_t>::const_iterator it = indices.begin();
         it != indices.end(); ++it) {
      const AutofillProfile* profile = profiles[*it];
      string16 field_text = profile->GetFieldText(AutofillType(*field));

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
  for (std::list<size_t>::const_iterator it = indices.begin();
       it != indices.end(); ++it) {
    const AutofillProfile* profile = profiles[*it];

    std::vector<AutofillFieldType> label_fields;
    bool found_differentiating_field = false;
    for (std::vector<AutofillFieldType>::const_iterator field = fields.begin();
         field != fields.end(); ++field) {
      // Skip over empty fields.
      string16 field_text = profile->GetFieldText(AutofillType(*field));
      if (field_text.empty())
        continue;

      std::map<string16, size_t>& field_text_frequencies =
          field_text_frequencies_by_field[*field];
      found_differentiating_field |=
          !field_text_frequencies.count(string16()) &&
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

    (*created_labels)[*it] =
        profile->ConstructInferredLabel(label_fields,
                                        label_fields.size());
  }
}

AutofillProfile::FormGroupList AutofillProfile::info_list() const {
  FormGroupList v(6);
  v[0] = &name_;
  v[1] = &email_;
  v[2] = &company_;
  v[3] = &home_number_;
  v[4] = &fax_number_;
  v[5] = &address_;
  return v;
}

AutofillProfile::FormGroupMap AutofillProfile::info_map() const {
  FormGroupMap m;
  m[AutofillType::NAME] = &name_;
  m[AutofillType::EMAIL] = &email_;
  m[AutofillType::COMPANY] = &company_;
  m[AutofillType::PHONE_HOME] = &home_number_;
  m[AutofillType::PHONE_FAX] = &fax_number_;
  m[AutofillType::ADDRESS_HOME] = &address_;
  return m;
}

AutofillProfile::MutableFormGroupMap AutofillProfile::mutable_info_map() {
  FormGroupMap m_const = info_map();
  MutableFormGroupMap m;
  for (FormGroupMap::const_iterator it = m_const.begin();
       it != m_const.end(); ++it) {
    m[it->first] = const_cast<FormGroup*>(it->second);
  }
  return m;
}

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile) {
  return os
      << UTF16ToUTF8(profile.Label())
      << " "
      << profile.guid()
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(NAME_FIRST)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(NAME_MIDDLE)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(NAME_LAST)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(EMAIL_ADDRESS)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(COMPANY_NAME)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE1)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE2)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(ADDRESS_HOME_CITY)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(ADDRESS_HOME_STATE)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(ADDRESS_HOME_ZIP)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(
             PHONE_HOME_WHOLE_NUMBER)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutofillType(
             PHONE_FAX_WHOLE_NUMBER)));
}
