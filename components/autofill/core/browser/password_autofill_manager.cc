// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/password_autofill_manager.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    AutofillDriver* autofill_driver) : autofill_driver_(autofill_driver) {
  DCHECK(autofill_driver);
}

PasswordAutofillManager::~PasswordAutofillManager() {
}

bool PasswordAutofillManager::DidAcceptAutofillSuggestion(
    const FormFieldData& field,
    const base::string16& username) {
  PasswordFormFillData password;
  if (!FindLoginInfo(field, &password))
    return false;

  if (WillFillUserNameAndPassword(username, password)) {
    autofill_driver_->RendererShouldAcceptPasswordAutofillSuggestion(username);
    return true;
  }

  return false;
}

void PasswordAutofillManager::AddPasswordFormMapping(
    const FormFieldData& username_element,
    const PasswordFormFillData& password) {
  login_to_password_info_[username_element] = password;
}

void PasswordAutofillManager::Reset() {
  login_to_password_info_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

bool PasswordAutofillManager::WillFillUserNameAndPassword(
    const base::string16& current_username,
    const PasswordFormFillData& fill_data) {
  // Look for any suitable matches to current field text.
  if (fill_data.basic_data.fields[0].value == current_username)
    return true;

  // Scan additional logins for a match.
   for (PasswordFormFillData::LoginCollection::const_iterator iter =
           fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end(); ++iter) {
    if (iter->first == current_username)
      return true;
  }

  for (PasswordFormFillData::UsernamesCollection::const_iterator usernames_iter
           = fill_data.other_possible_usernames.begin();
       usernames_iter != fill_data.other_possible_usernames.end();
       ++usernames_iter) {
    for (size_t i = 0; i < usernames_iter->second.size(); ++i) {
      if (usernames_iter->second[i] == current_username)
        return true;
    }
  }

  return false;
}

bool PasswordAutofillManager::FindLoginInfo(
    const FormFieldData& field,
    PasswordFormFillData* found_password) {
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(field);
  if (iter == login_to_password_info_.end())
    return false;

  *found_password = iter->second;
  return true;
}

}  // namespace autofill
