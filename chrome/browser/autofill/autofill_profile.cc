// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_profile.h"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <vector>

#include "app/l10n_util.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/contact_info.h"
#include "chrome/browser/autofill/fax_number.h"
#include "chrome/browser/autofill/home_address.h"
#include "chrome/browser/autofill/home_phone_number.h"
#include "chrome/common/guid.h"
#include "grit/generated_resources.h"

namespace {

void InitPersonalInfo(FormGroupMap* personal_info) {
  (*personal_info)[AutoFillType::CONTACT_INFO] = new ContactInfo();
  (*personal_info)[AutoFillType::PHONE_HOME] = new HomePhoneNumber();
  (*personal_info)[AutoFillType::PHONE_FAX] = new FaxNumber();
  (*personal_info)[AutoFillType::ADDRESS_HOME] = new HomeAddress();
}

// Maps |field_type| to a field type that can be directly stored in a profile
// (in the sense that it makes sense to call |AutoFillProfile::SetInfo()| with
// the returned field type as the first parameter.
AutoFillFieldType GetEquivalentFieldType(AutoFillFieldType field_type) {
  // When billing information is requested from the profile we map to the
  // home address equivalents.
  switch (field_type) {
    case ADDRESS_BILLING_LINE1:
      return ADDRESS_HOME_LINE1;

    case ADDRESS_BILLING_LINE2:
      return ADDRESS_HOME_LINE2;

    case ADDRESS_BILLING_APT_NUM:
      return ADDRESS_HOME_APT_NUM;

    case ADDRESS_BILLING_CITY:
      return ADDRESS_HOME_CITY;

    case ADDRESS_BILLING_STATE:
      return ADDRESS_HOME_STATE;

    case ADDRESS_BILLING_ZIP:
      return ADDRESS_HOME_ZIP;

    case ADDRESS_BILLING_COUNTRY:
      return ADDRESS_HOME_COUNTRY;

    default:
      return field_type;
  }
}

// Like |GetEquivalentFieldType()| above, but also returns |NAME_FULL| for
// first, middle, and last name field types.
AutoFillFieldType GetEquivalentFieldTypeCollapsingNames(
    AutoFillFieldType field_type) {
  if (field_type == NAME_FIRST || field_type == NAME_MIDDLE ||
      field_type == NAME_LAST)
    return NAME_FULL;

  return GetEquivalentFieldType(field_type);
}

// Fills |distinguishing_fields| with a list of fields to use when creating
// labels that can help to distinguish between two profiles. Draws fields from
// |suggested_fields| if it is non-NULL; otherwise returns a default list.
// If |suggested_fields| is non-NULL, does not include |excluded_field| in the
// list. Otherwise, |excluded_field| is ignored, and should be set to
// |UNKNOWN_TYPE| by convention. The resulting list of fields is sorted in
// decreasing order of importance.
void GetFieldsForDistinguishingProfiles(
    const std::vector<AutoFillFieldType>* suggested_fields,
    AutoFillFieldType excluded_field,
    std::vector<AutoFillFieldType>* distinguishing_fields) {
  static const AutoFillFieldType kDefaultDistinguishingFields[] = {
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
  std::set<AutoFillFieldType> seen_fields;
  seen_fields.insert(UNKNOWN_TYPE);
  seen_fields.insert(GetEquivalentFieldTypeCollapsingNames(excluded_field));

  distinguishing_fields->clear();
  for (std::vector<AutoFillFieldType>::const_iterator it =
           suggested_fields->begin();
       it != suggested_fields->end(); ++it) {
    AutoFillFieldType suggested_type =
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
    for (std::vector<AutoFillFieldType>::const_iterator it =
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

AutoFillProfile::AutoFillProfile(const std::string& guid)
    : guid_(guid) {
  InitPersonalInfo(&personal_info_);
}

AutoFillProfile::AutoFillProfile()
    : guid_(guid::GenerateGUID()) {
  InitPersonalInfo(&personal_info_);
}

AutoFillProfile::AutoFillProfile(const AutoFillProfile& source)
    : FormGroup() {
  operator=(source);
}

AutoFillProfile::~AutoFillProfile() {
  STLDeleteContainerPairSecondPointers(personal_info_.begin(),
                                       personal_info_.end());
}

void AutoFillProfile::GetPossibleFieldTypes(
    const string16& text,
    FieldTypeSet* possible_types) const {
  for (FormGroupMap::const_iterator iter = personal_info_.begin();
       iter != personal_info_.end(); ++iter) {
    FormGroup* data = iter->second;
    DCHECK(data != NULL);
    data->GetPossibleFieldTypes(text, possible_types);
  }
}

void AutoFillProfile::GetAvailableFieldTypes(
    FieldTypeSet* available_types) const {
  for (FormGroupMap::const_iterator iter = personal_info_.begin();
       iter != personal_info_.end(); ++iter) {
    FormGroup* data = iter->second;
    DCHECK(data != NULL);
    data->GetAvailableFieldTypes(available_types);
  }
}

string16 AutoFillProfile::GetFieldText(const AutoFillType& type) const {
  AutoFillType return_type(GetEquivalentFieldType(type.field_type()));
  FormGroupMap::const_iterator iter = personal_info_.find(return_type.group());
  if (iter == personal_info_.end() || iter->second == NULL)
    return string16();

  return iter->second->GetFieldText(return_type);
}

void AutoFillProfile::FindInfoMatches(
    const AutoFillType& type,
    const string16& info,
    std::vector<string16>* matched_text) const {
  if (matched_text == NULL) {
    DLOG(ERROR) << "NULL matched text passed in";
    return;
  }

  string16 clean_info = StringToLowerASCII(CollapseWhitespace(info, false));

  // If the field_type is unknown, then match against all field types.
  if (type.field_type() == UNKNOWN_TYPE) {
    FormGroupMap::const_iterator iter;
    for (iter = personal_info_.begin(); iter != personal_info_.end(); ++iter) {
      iter->second->FindInfoMatches(type, clean_info, matched_text);
    }
  } else {
    FormGroupMap::const_iterator iter = personal_info_.find(type.group());
    DCHECK(iter != personal_info_.end() && iter->second != NULL);
    if (iter != personal_info_.end() && iter->second != NULL)
      iter->second->FindInfoMatches(type, clean_info, matched_text);
  }
}

void AutoFillProfile::SetInfo(const AutoFillType& type, const string16& value) {
  FormGroupMap::const_iterator iter = personal_info_.find(type.group());
  if (iter == personal_info_.end() || iter->second == NULL)
    return;

  iter->second->SetInfo(type, CollapseWhitespace(value, false));
}

FormGroup* AutoFillProfile::Clone() const {
  return new AutoFillProfile(*this);
}

const string16 AutoFillProfile::Label() const {
  return label_;
}

// static
bool AutoFillProfile::AdjustInferredLabels(
    std::vector<AutoFillProfile*>* profiles) {
  const size_t kMinimalFieldsShown = 2;

  std::vector<string16> created_labels;
  CreateInferredLabels(profiles, NULL, UNKNOWN_TYPE, kMinimalFieldsShown,
                       &created_labels);
  DCHECK_EQ(profiles->size(), created_labels.size());

  bool updated_labels = false;
  for (size_t i = 0; i < profiles->size(); ++i) {
    if ((*profiles)[i]->Label() != created_labels[i]) {
      updated_labels = true;
      (*profiles)[i]->set_label(created_labels[i]);
    }
  }
  return updated_labels;
}

// static
void AutoFillProfile::CreateInferredLabels(
    const std::vector<AutoFillProfile*>* profiles,
    const std::vector<AutoFillFieldType>* suggested_fields,
    AutoFillFieldType excluded_field,
    size_t minimal_fields_shown,
    std::vector<string16>* created_labels) {
  DCHECK(profiles);
  DCHECK(created_labels);

  std::vector<AutoFillFieldType> fields_to_use;
  GetFieldsForDistinguishingProfiles(suggested_fields, excluded_field,
                                     &fields_to_use);

  // First non empty data are always present in the label, even if it matches.
  created_labels->resize(profiles->size());
  std::map<string16, std::list<size_t> > labels;
  for (size_t it = 0; it < profiles->size(); ++it) {
    labels[profiles->at(it)->GetFieldText(
        AutoFillType(fields_to_use.size() ? fields_to_use[0] :
                                            NAME_FULL))].push_back(it);
  }
  std::map<string16, std::list<size_t> >::iterator label_iterator;
  for (label_iterator = labels.begin(); label_iterator != labels.end();
       ++label_iterator) {
    if (label_iterator->second.size() > 1) {
      // We have more than one item with the same preview, add differentiating
      // data.
      std::list<size_t>::iterator similar_profiles;
      DCHECK(fields_to_use.size() <= MAX_VALID_FIELD_TYPE);
      std::map<string16, int> tested_fields[MAX_VALID_FIELD_TYPE];
      for (similar_profiles = label_iterator->second.begin();
           similar_profiles != label_iterator->second.end();
           ++similar_profiles) {
        for (size_t i = 0; i < fields_to_use.size(); ++i) {
          string16 key = profiles->at(*similar_profiles)->GetFieldText(
              AutoFillType(fields_to_use[i]));
          std::map<string16, int>::iterator tested_field =
              tested_fields[i].find(key);
          if (tested_field == tested_fields[i].end())
            (tested_fields[i])[key] = 1;
          else
            ++(tested_field->second);
        }
      }
      std::vector<AutoFillFieldType> fields;
      std::vector<AutoFillFieldType> first_non_empty_fields;
      size_t added_fields = 0;
      bool matched_necessary = false;
      // Leave it as a candidate if it is not the same for everybody.
      for (size_t i = 0; i < fields_to_use.size(); ++i) {
        if (tested_fields[i].size() == label_iterator->second.size()) {
          // This field is different for everybody.
          if (!matched_necessary) {
            matched_necessary = true;
            fields.clear();
            added_fields = 1;
            if (first_non_empty_fields.size()) {
              added_fields += first_non_empty_fields.size();
              fields.resize(added_fields - 1);
              std::copy(first_non_empty_fields.begin(),
                        first_non_empty_fields.end(),
                        fields.begin());
            }
          } else {
            ++added_fields;
          }
          fields.push_back(fields_to_use[i]);
          if (added_fields >= minimal_fields_shown)
            break;
        } else if (tested_fields[i].size() != 1) {
          // this field is different for some.
          if (added_fields < minimal_fields_shown) {
            first_non_empty_fields.push_back(fields_to_use[i]);
            ++added_fields;
            if (added_fields == minimal_fields_shown && matched_necessary)
              break;
          }
          fields.push_back(fields_to_use[i]);
        } else if (added_fields < minimal_fields_shown &&
                   !tested_fields[i].count(string16())) {
          fields.push_back(fields_to_use[i]);
          first_non_empty_fields.push_back(fields_to_use[i]);
          ++added_fields;
          if (added_fields == minimal_fields_shown && matched_necessary)
            break;
        }
      }
      // Update labels if needed.
      for (similar_profiles = label_iterator->second.begin();
           similar_profiles != label_iterator->second.end();
           ++similar_profiles) {
        size_t field_it = 0;
        for (size_t field_id = 0;
             field_id < fields_to_use.size() &&
             field_it < fields.size(); ++field_id) {
          if (fields[field_it] == fields_to_use[field_id]) {
            if ((tested_fields[field_id])[
                profiles->at(*similar_profiles)->GetFieldText(
                AutoFillType(fields_to_use[field_id]))] == 1) {
              // this field is unique among the subset.
              break;
            }
            ++field_it;
          }
        }

        string16 new_label;
        if (field_it < fields.size() && fields.size() > minimal_fields_shown) {
          std::vector<AutoFillFieldType> unique_fields;
          unique_fields.resize(fields.size());
          std::copy(fields.begin(), fields.end(), unique_fields.begin());
          unique_fields.resize(std::max(field_it + 1, minimal_fields_shown));
          new_label =
              profiles->at(*similar_profiles)->ConstructInferredLabel(
                  unique_fields);
        } else {
          new_label =
              profiles->at(*similar_profiles)->ConstructInferredLabel(fields);
        }
        (*created_labels)[*similar_profiles] = new_label;
      }
    } else {
      std::vector<AutoFillFieldType> non_empty_fields;
      size_t include_fields = minimal_fields_shown ? minimal_fields_shown : 1;
      non_empty_fields.reserve(include_fields);
      for (size_t i = 0; i < fields_to_use.size(); ++i) {
        if (!profiles->at(label_iterator->second.front())->GetFieldText(
            AutoFillType(fields_to_use[i])).empty()) {
          non_empty_fields.push_back(fields_to_use[i]);
          if (non_empty_fields.size() >= include_fields)
            break;
        }
      }

      (*created_labels)[label_iterator->second.front()] =
          profiles->at(label_iterator->second.front())->ConstructInferredLabel(
              non_empty_fields);
    }
  }
}

bool AutoFillProfile::IsEmpty() const {
  FieldTypeSet types;
  GetAvailableFieldTypes(&types);
  return types.empty();
}

void AutoFillProfile::operator=(const AutoFillProfile& source) {
  if (this == &source)
    return;

  label_ = source.label_;
  guid_ = source.guid_;

  STLDeleteContainerPairSecondPointers(personal_info_.begin(),
                                       personal_info_.end());
  personal_info_.clear();

  FormGroupMap::const_iterator iter;
  for (iter = source.personal_info_.begin();
       iter != source.personal_info_.end();
       ++iter) {
    personal_info_[iter->first] = iter->second->Clone();
  }
}

int AutoFillProfile::Compare(const AutoFillProfile& profile) const {
  // The following AutoFill field types are the only types we store in the WebDB
  // so far, so we're only concerned with matching these types in the profile.
  const AutoFillFieldType types[] = { NAME_FIRST,
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
    int comparison = GetFieldText(AutoFillType(types[index])).compare(
        profile.GetFieldText(AutoFillType(types[index])));
    if (comparison != 0)
      return comparison;
  }

  return 0;
}

bool AutoFillProfile::operator==(const AutoFillProfile& profile) const {
  if (label_ != profile.label_ || guid_ != profile.guid_)
    return false;

  return Compare(profile) == 0;
}

bool AutoFillProfile::operator!=(const AutoFillProfile& profile) const {
  return !operator==(profile);
}

const string16 AutoFillProfile::PrimaryValue() const {
  return GetFieldText(AutoFillType(NAME_FULL)) +
         GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)) +
         GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)) +
         GetFieldText(AutoFillType(EMAIL_ADDRESS));
}

string16 AutoFillProfile::ConstructInferredLabel(
    const std::vector<AutoFillFieldType>& included_fields) const {
  const string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_SUMMARY_SEPARATOR);

  string16 label;
  for (std::vector<AutoFillFieldType>::const_iterator it =
           included_fields.begin();
       it != included_fields.end(); ++it) {
    string16 field = GetFieldText(AutoFillType(*it));
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
  }
  return label;
}

// So we can compare AutoFillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutoFillProfile& profile) {
  return os
      << UTF16ToUTF8(profile.Label())
      << " "
      << profile.guid()
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(NAME_FIRST)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(NAME_MIDDLE)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(NAME_LAST)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(EMAIL_ADDRESS)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(COMPANY_NAME)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_CITY)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_STATE)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(ADDRESS_HOME_COUNTRY)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(
             PHONE_HOME_WHOLE_NUMBER)))
      << " "
      << UTF16ToUTF8(profile.GetFieldText(AutoFillType(
             PHONE_FAX_WHOLE_NUMBER)));
}
