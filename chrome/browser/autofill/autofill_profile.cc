// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_profile.h"

#include <algorithm>
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
#include "grit/generated_resources.h"

namespace {

void InitPersonalInfo(FormGroupMap* personal_info) {
  (*personal_info)[AutoFillType::CONTACT_INFO] = new ContactInfo();
  (*personal_info)[AutoFillType::PHONE_HOME] = new HomePhoneNumber();
  (*personal_info)[AutoFillType::PHONE_FAX] = new FaxNumber();
  (*personal_info)[AutoFillType::ADDRESS_HOME] = new HomeAddress();
}

}  // namespace

AutoFillProfile::AutoFillProfile(const string16& label, int unique_id)
    : label_(label),
      unique_id_(unique_id) {
  InitPersonalInfo(&personal_info_);
}

AutoFillProfile::AutoFillProfile()
    : unique_id_(0) {
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
  AutoFillType return_type = type;

  // When billing information is requested from the profile we map to the
  // home address equivalents.  This indicates the address information within
  // this profile is being used to fill billing fields in the form.
  switch (type.field_type()) {
    case ADDRESS_BILLING_LINE1:
      return_type = AutoFillType(ADDRESS_HOME_LINE1);
      break;
    case ADDRESS_BILLING_LINE2:
      return_type = AutoFillType(ADDRESS_HOME_LINE2);
      break;
    case ADDRESS_BILLING_APT_NUM:
      return_type = AutoFillType(ADDRESS_HOME_APT_NUM);
      break;
    case ADDRESS_BILLING_CITY:
      return_type = AutoFillType(ADDRESS_HOME_CITY);
      break;
    case ADDRESS_BILLING_STATE:
      return_type = AutoFillType(ADDRESS_HOME_STATE);
      break;
    case ADDRESS_BILLING_ZIP:
      return_type = AutoFillType(ADDRESS_HOME_ZIP);
      break;
    case ADDRESS_BILLING_COUNTRY:
      return_type = AutoFillType(ADDRESS_HOME_COUNTRY);
      break;
    default:
      break;
  }

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
  AutoFillProfile* profile = new AutoFillProfile();
  profile->label_ = label_;
  profile->unique_id_ = unique_id();

  FormGroupMap::const_iterator iter;
  for (iter = personal_info_.begin(); iter != personal_info_.end(); ++iter) {
    if (profile->personal_info_.count(iter->first))
      delete profile->personal_info_[iter->first];
    profile->personal_info_[iter->first] = iter->second->Clone();
  }

  return profile;
}

string16 AutoFillProfile::PreviewSummary() const {
  // Fetch the components of the summary string.  Any or all of these
  // may be an empty string.
  string16 first_name = GetFieldText(AutoFillType(NAME_FIRST));
  string16 last_name = GetFieldText(AutoFillType(NAME_LAST));
  string16 address = GetFieldText(AutoFillType(ADDRESS_HOME_LINE1));

  // String separators depend (below) on the existence of the various fields.
  bool have_first_name = first_name.length() > 0;
  bool have_last_name = last_name.length() > 0;
  bool have_address = address.length() > 0;

  // Name separator defaults to "".  Space if we have first and last name.
  string16 name_separator;
  if (have_first_name && have_last_name) {
    name_separator = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_ADDRESS_NAME_SEPARATOR);
  }

  // E.g. "John Smith", or "John", or "Smith", or "".
  string16 name_format = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DIALOG_ADDRESS_SUMMARY_NAME_FORMAT,
      first_name,
      name_separator,
      last_name);

  // Summary separator defaults to "".  ", " if we have name and address.
  string16 summary_separator;
  if ((have_first_name || have_last_name) && have_address) {
    summary_separator = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_ADDRESS_SUMMARY_SEPARATOR);
  }

  // E.g. "John Smith, 123 Main Street".
  string16 summary_format = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DIALOG_ADDRESS_SUMMARY_FORMAT,
      name_format,
      summary_separator,
      address);

  return summary_format;
}

void AutoFillProfile::operator=(const AutoFillProfile& source) {
  label_ = source.label_;
  unique_id_ = source.unique_id_;

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

bool AutoFillProfile::operator==(const AutoFillProfile& profile) const {
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

  if (label_ != profile.label_ ||
      unique_id_ != profile.unique_id_) {
    return false;
  }

  for (size_t index = 0; index < arraysize(types); ++index) {
    if (GetFieldText(AutoFillType(types[index])) !=
        profile.GetFieldText(AutoFillType(types[index])))
      return false;
  }

  return true;
}

bool AutoFillProfile::operator!=(const AutoFillProfile& profile) const {
  return !operator==(profile);
}

Address* AutoFillProfile::GetHomeAddress() {
  return static_cast<Address*>(personal_info_[AutoFillType::ADDRESS_HOME]);
}

// So we can compare AutoFillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutoFillProfile& profile) {
  return os
      << UTF16ToUTF8(profile.Label())
      << " "
      << profile.unique_id()
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
