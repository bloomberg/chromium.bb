// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_profile.h"

#include <vector>

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/billing_address.h"
#include "chrome/browser/autofill/contact_info.h"
#include "chrome/browser/autofill/fax_number.h"
#include "chrome/browser/autofill/home_address.h"
#include "chrome/browser/autofill/home_phone_number.h"

AutoFillProfile::AutoFillProfile(const string16& label, int unique_id)
    : label_(label),
      unique_id_(unique_id),
      use_billing_address_(true) {
  personal_info_[AutoFillType::CONTACT_INFO] = new ContactInfo();
  personal_info_[AutoFillType::PHONE_HOME] = new HomePhoneNumber();
  personal_info_[AutoFillType::PHONE_FAX] = new FaxNumber();
  personal_info_[AutoFillType::ADDRESS_HOME] = new HomeAddress();
  personal_info_[AutoFillType::ADDRESS_BILLING] = new BillingAddress();
}

AutoFillProfile::AutoFillProfile()
    : unique_id_(0),
      use_billing_address_(true) {
}

AutoFillProfile::AutoFillProfile(const AutoFillProfile& source) {
  operator=(source);
}

AutoFillProfile::~AutoFillProfile() {
  STLDeleteContainerPairSecondPointers(personal_info_.begin(),
                                       personal_info_.end());
}

void AutoFillProfile::GetPossibleFieldTypes(
    const string16& text,
    FieldTypeSet* possible_types) const {
  FormGroupMap::const_iterator iter;
  for (iter = personal_info_.begin(); iter != personal_info_.end(); ++iter) {
    FormGroup* data = iter->second;
    DCHECK(data != NULL);
    if (data != NULL)
      data->GetPossibleFieldTypes(text, possible_types);
  }
}

string16 AutoFillProfile::GetFieldText(const AutoFillType& type) const {
  FormGroupMap::const_iterator iter = personal_info_.find(type.group());
  if (iter == personal_info_.end() || iter->second == NULL)
    return string16();

  return iter->second->GetFieldText(type);
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
  profile->use_billing_address_ = use_billing_address_;

  FormGroupMap::const_iterator iter;
  for (iter = personal_info_.begin(); iter != personal_info_.end(); ++iter) {
    profile->personal_info_[iter->first] = iter->second->Clone();
  }

  return profile;
}

void AutoFillProfile::operator=(const AutoFillProfile& source) {
  label_ = source.label_;
  unique_id_ = source.unique_id_;
  use_billing_address_ = source.use_billing_address_;

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

void AutoFillProfile::set_use_billing_address(bool use) {
  if (use_billing_address_ == use)
    return;

  Address* billing_address = GetBillingAddress();

  if (use) {
    // If we were using the home address as a billing address then the home
    // address information should be cleared out of the billing address object.
    billing_address->Clear();
  } else {
    // If we no longer want to use an alternate billing address then clone the
    // home address information for our billing address.
    Address* home_address = GetHomeAddress();
    billing_address->Clone(*home_address);
  }

  use_billing_address_ = use;
}

Address* AutoFillProfile::GetBillingAddress() {
  return static_cast<Address*>(personal_info_[AutoFillType::ADDRESS_BILLING]);
}

Address* AutoFillProfile::GetHomeAddress() {
  return static_cast<Address*>(personal_info_[AutoFillType::ADDRESS_HOME]);
}
