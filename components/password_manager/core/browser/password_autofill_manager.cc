// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    password_manager::PasswordManagerClient* password_manager_client,
    autofill::AutofillManagerDelegate* autofill_manager_delegate)
    : password_manager_client_(password_manager_client),
      autofill_manager_delegate_(autofill_manager_delegate),
      weak_ptr_factory_(this) {
}

PasswordAutofillManager::~PasswordAutofillManager() {
}

bool PasswordAutofillManager::AcceptSuggestion(
    const autofill::FormFieldData& field,
    const base::string16& username) {
  autofill::PasswordFormFillData fill_data;
  base::string16 password;
  if (FindLoginInfo(field, &fill_data) &&
      GetPasswordForUsername(username, fill_data, &password)) {
    PasswordManagerDriver* driver = password_manager_client_->GetDriver();
    driver->AcceptPasswordAutofillSuggestion(username, password);
    return true;
  }
  return false;
}

void PasswordAutofillManager::OnAddPasswordFormMapping(
    const autofill::FormFieldData& field,
    const autofill::PasswordFormFillData& fill_data) {
  if (!autofill::IsValidFormFieldData(field) ||
      !autofill::IsValidPasswordFormFillData(fill_data))
    return;

  login_to_password_info_[field] = fill_data;
}

void PasswordAutofillManager::OnShowPasswordSuggestions(
    const autofill::FormFieldData& field,
    const gfx::RectF& bounds,
    const std::vector<base::string16>& suggestions,
    const std::vector<base::string16>& realms) {
  if (!autofill::IsValidString16Vector(suggestions) ||
      !autofill::IsValidString16Vector(realms) ||
      suggestions.size() != realms.size())
    return;

  form_field_ = field;

  if (suggestions.empty()) {
    autofill_manager_delegate_->HideAutofillPopup();
    return;
  }

  std::vector<base::string16> empty(suggestions.size());
  std::vector<int> password_ids(suggestions.size(),
                                autofill::POPUP_ITEM_ID_PASSWORD_ENTRY);
  autofill_manager_delegate_->ShowAutofillPopup(
      bounds,
      field.text_direction,
      suggestions,
      realms,
      empty,
      password_ids,
      weak_ptr_factory_.GetWeakPtr());
}

void PasswordAutofillManager::Reset() {
  login_to_password_info_.clear();
}

bool PasswordAutofillManager::AcceptSuggestionForTest(
    const autofill::FormFieldData& field,
    const base::string16& username) {
  return AcceptSuggestion(field, username);
}

void PasswordAutofillManager::OnPopupShown() {
}

void PasswordAutofillManager::OnPopupHidden() {
}

void PasswordAutofillManager::DidSelectSuggestion(const base::string16& value,
                                                  int identifier) {
  // This is called to preview an autofill suggestion, but we don't currently
  // do that for password forms (crbug.com/63421). If it is ever implemented,
  // ClearPreviewedForm() must also be implemented().
}

void PasswordAutofillManager::DidAcceptSuggestion(const base::string16& value,
                                                  int identifier) {
  if (!AcceptSuggestion(form_field_, value))
    NOTREACHED();
  autofill_manager_delegate_->HideAutofillPopup();
}

void PasswordAutofillManager::RemoveSuggestion(const base::string16& value,
                                               int identifier) {
  NOTREACHED();
}

void PasswordAutofillManager::ClearPreviewedForm() {
  // There is currently no preview for password autofill (crbug.com/63421).
  // This function needs an implemention if preview is ever implemented.
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

bool PasswordAutofillManager::GetPasswordForUsername(
    const base::string16& current_username,
    const autofill::PasswordFormFillData& fill_data,
    base::string16* password) {
  // TODO(dubroy): When password access requires some kind of authentication
  // (e.g. Keychain access on Mac OS), use |password_manager_client_| here to
  // fetch the actual password. See crbug.com/178358 for more context.

  // Look for any suitable matches to current field text.
  if (fill_data.basic_data.fields[0].value == current_username) {
    *password = fill_data.basic_data.fields[1].value;
    return true;
  }

  // Scan additional logins for a match.
  for (autofill::PasswordFormFillData::LoginCollection::const_iterator iter =
           fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end();
       ++iter) {
    if (iter->first == current_username) {
      *password = iter->second.password;
      return true;
    }
  }

  for (autofill::PasswordFormFillData::UsernamesCollection::const_iterator
           usernames_iter = fill_data.other_possible_usernames.begin();
       usernames_iter != fill_data.other_possible_usernames.end();
       ++usernames_iter) {
    for (size_t i = 0; i < usernames_iter->second.size(); ++i) {
      if (usernames_iter->second[i] == current_username) {
        *password = usernames_iter->first.password;
        return true;
      }
    }
  }

  return false;
}

bool PasswordAutofillManager::FindLoginInfo(
    const autofill::FormFieldData& field,
    autofill::PasswordFormFillData* found_password) {
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(field);
  if (iter == login_to_password_info_.end())
    return false;

  *found_password = iter->second;
  return true;
}

}  //  namespace password_manager
