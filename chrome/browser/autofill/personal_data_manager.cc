// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager.h"

#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/form_structure.h"

// The minimum number of fields that must contain user data and have known types
// before autofill will attempt to import the data into a profile.
static const int kMinImportSize = 5;

// The number of digits in a phone number.
static const int kPhoneNumberLength = 7;

// The number of digits in an area code.
static const int kPhoneCityCodeLength = 3;

PersonalDataManager::~PersonalDataManager() {
}

bool PersonalDataManager::ImportFormData(
    const std::vector<FormStructure*>& form_structures,
    AutoFillManager* autofill_manager) {
  InitializeIfNeeded();

  // Parse the form and construct a profile based on the information that is
  // possible to import.
  int importable_fields = 0;
  int importable_credit_card_fields = 0;
  imported_profile_.reset(new AutoFillProfile(string16(),
                                              CreateNextUniqueId()));
  imported_credit_card_.reset(new CreditCard(string16()));

  bool billing_address_info = false;
  std::vector<FormStructure*>::const_iterator iter;
  for (iter = form_structures.begin(); iter != form_structures.end(); ++iter) {
    const FormStructure* form = *iter;
    for (size_t i = 0; i < form->field_count(); ++i) {
      const AutoFillField* field = form->field(i);
      string16 value = CollapseWhitespace(field->value(), false);

      // If we don't know the type of the field, or the user hasn't entered any
      // information into the field, then skip it.
      if (!field->IsFieldFillable() || value.empty())
        continue;

      AutoFillType field_type(field->type());
      FieldTypeGroup group(field_type.group());

      if (group == AutoFillType::CREDIT_CARD) {
        // If the user has a password set, we have no way of setting credit
        // card numbers.
        if (!HasPassword()) {
          imported_credit_card_->SetInfo(AutoFillType(field_type.field_type()),
                                         value);
          ++importable_credit_card_fields;
        }
      } else {
        // In the case of a phone number, if the whole phone number was entered
        // into a single field, then parse it and set the sub components.
        if (field_type.subgroup() == AutoFillType::PHONE_WHOLE_NUMBER) {
          if (group == AutoFillType::PHONE_HOME) {
            ParsePhoneNumber(imported_profile_.get(), &value, PHONE_HOME_NUMBER,
                             PHONE_HOME_CITY_CODE, PHONE_HOME_COUNTRY_CODE);
          } else if (group == AutoFillType::PHONE_FAX) {
            ParsePhoneNumber(imported_profile_.get(), &value, PHONE_FAX_NUMBER,
                             PHONE_FAX_CITY_CODE, PHONE_FAX_COUNTRY_CODE);
          }
          continue;
        }

        imported_profile_->SetInfo(AutoFillType(field_type.field_type()),
                                   value);

        // If we found any billing address information, then set the profile to
        // use a separate billing address.
        if (group == AutoFillType::ADDRESS_BILLING)
          billing_address_info = true;

        ++importable_fields;
      }
    }
  }

  // If the user did not enter enough information on the page then don't bother
  // importing the data.
  if (importable_fields + importable_credit_card_fields < kMinImportSize)
    return false;

  if (!billing_address_info)
    imported_profile_->set_use_billing_address(false);

  if (importable_fields == 0)
    imported_profile_.reset();

  if (importable_credit_card_fields == 0)
    imported_credit_card_.reset();

  // TODO(jhawkins): Alert the AutoFillManager that we have data.

  return true;
}

void PersonalDataManager::GetPossibleFieldTypes(const string16& text,
                                                FieldTypeSet* possible_types) {
  InitializeIfNeeded();

  string16 clean_info = StringToLowerASCII(CollapseWhitespace(text, false));

  if (clean_info.empty()) {
    possible_types->insert(EMPTY_TYPE);
    return;
  }

  ScopedVector<FormGroup>::const_iterator iter;
  for (iter = profiles_.begin(); iter != profiles_.end(); ++iter) {
    const FormGroup* profile = *iter;
    if (!profile) {
      DLOG(ERROR) << "NULL information in profiles list";
      continue;
    }
    profile->GetPossibleFieldTypes(clean_info, possible_types);
  }

  for (iter = credit_cards_.begin(); iter != credit_cards_.end(); ++iter) {
    const FormGroup* credit_card = *iter;
    if (!credit_card) {
      DLOG(ERROR) << "NULL information in credit cards list";
      continue;
    }
    credit_card->GetPossibleFieldTypes(clean_info, possible_types);
  }

  if (possible_types->size() == 0)
    possible_types->insert(UNKNOWN_TYPE);
}

bool PersonalDataManager::HasPassword() {
  InitializeIfNeeded();
  return !password_hash_.empty();
}

PersonalDataManager::PersonalDataManager()
    : is_initialized_(false) {
}

void PersonalDataManager::InitializeIfNeeded() {
  if (is_initialized_)
    return;

  is_initialized_ = true;
  // TODO(jhawkins): Load data.
}

int PersonalDataManager::CreateNextUniqueId() {
  // Profile IDs MUST start at 1 to allow 0 as an error value when reading
  // the ID from the WebDB (see LoadData()).
  int id = 1;
  while (unique_ids_.count(id) != 0)
    ++id;
  unique_ids_.insert(id);
  return id;
}

void PersonalDataManager::ParsePhoneNumber(
    AutoFillProfile* profile,
    string16* value,
    AutoFillFieldType number,
    AutoFillFieldType city_code,
    AutoFillFieldType country_code) const {
  // Treat the last 7 digits as the number.
  string16 number_str = value->substr(kPhoneNumberLength,
                                      value->size() - kPhoneNumberLength);
  profile->SetInfo(AutoFillType(number), number_str);
  value->resize(value->size() - kPhoneNumberLength);
  if (value->empty())
    return;

  // Treat the next three digits as the city code.
  string16 city_code_str = value->substr(kPhoneCityCodeLength,
                                         value->size() - kPhoneCityCodeLength);
  profile->SetInfo(AutoFillType(city_code), city_code_str);
  value->resize(value->size() - kPhoneCityCodeLength);
  if (value->empty())
    return;

  // Treat any remaining digits as the country code.
  profile->SetInfo(AutoFillType(country_code), *value);
}
