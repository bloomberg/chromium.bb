// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

namespace {

// This function attempts to fill |suggestions| and |realms| form |fill_data|
// based on |current_username|. Unless |show_all| is true, it only picks
// suggestions where the username has |current_username| as a prefix.
void GetSuggestions(const autofill::PasswordFormFillData& fill_data,
                    const base::string16& current_username,
                    std::vector<base::string16>* suggestions,
                    std::vector<base::string16>* realms,
                    bool show_all) {
  if (show_all ||
      StartsWith(fill_data.username_field.value, current_username, false)) {
    suggestions->push_back(fill_data.username_field.value);
    realms->push_back(base::UTF8ToUTF16(fill_data.preferred_realm));
  }

  for (const auto& login : fill_data.additional_logins) {
    if (show_all || StartsWith(login.first, current_username, false)) {
      suggestions->push_back(login.first);
      realms->push_back(base::UTF8ToUTF16(login.second.realm));
    }
  }

  for (const auto& usernames : fill_data.other_possible_usernames) {
    for (size_t i = 0; i < usernames.second.size(); ++i) {
      if (show_all ||
          StartsWith(usernames.second[i], current_username, false)) {
        suggestions->push_back(usernames.second[i]);
        realms->push_back(base::UTF8ToUTF16(usernames.first.realm));
      }
    }
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    PasswordManagerClient* password_manager_client,
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client)
    : password_manager_client_(password_manager_client),
      password_manager_driver_(password_manager_driver),
      autofill_client_(autofill_client),
      weak_ptr_factory_(this) {
}

PasswordAutofillManager::~PasswordAutofillManager() {
}

bool PasswordAutofillManager::FillSuggestion(int key,
                                             const base::string16& username) {
  autofill::PasswordFormFillData fill_data;
  base::string16 password;
  if (FindLoginInfo(key, &fill_data) &&
      GetPasswordForUsername(username, fill_data, &password)) {
    password_manager_driver_->FillSuggestion(username, password);
    return true;
  }
  return false;
}

bool PasswordAutofillManager::PreviewSuggestion(
    int key,
    const base::string16& username) {
  autofill::PasswordFormFillData fill_data;
  base::string16 password;
  if (FindLoginInfo(key, &fill_data) &&
      GetPasswordForUsername(username, fill_data, &password)) {
    password_manager_driver_->PreviewSuggestion(username, password);
    return true;
  }
  return false;
}

void PasswordAutofillManager::OnAddPasswordFormMapping(
    int key,
    const autofill::PasswordFormFillData& fill_data) {
  if (!autofill::IsValidPasswordFormFillData(fill_data))
    return;

  login_to_password_info_[key] = fill_data;
}

void PasswordAutofillManager::OnShowPasswordSuggestions(
    int key,
    base::i18n::TextDirection text_direction,
    const base::string16& typed_username,
    bool show_all,
    const gfx::RectF& bounds) {
  std::vector<base::string16> suggestions;
  std::vector<base::string16> realms;
  LoginToPasswordInfoMap::const_iterator fill_data_it =
      login_to_password_info_.find(key);
  if (fill_data_it == login_to_password_info_.end()) {
    // Probably a compromised renderer.
    NOTREACHED();
    return;
  }
  GetSuggestions(fill_data_it->second, typed_username, &suggestions, &realms,
                 show_all);
  DCHECK_EQ(suggestions.size(), realms.size());

  form_data_key_ = key;

  if (suggestions.empty()) {
    autofill_client_->HideAutofillPopup();
    return;
  }

  std::vector<base::string16> empty(suggestions.size());
  std::vector<int> password_ids(suggestions.size(),
                                autofill::POPUP_ITEM_ID_PASSWORD_ENTRY);
  autofill_client_->ShowAutofillPopup(bounds,
                                      text_direction,
                                      suggestions,
                                      realms,
                                      empty,
                                      password_ids,
                                      weak_ptr_factory_.GetWeakPtr());
}

void PasswordAutofillManager::Reset() {
  login_to_password_info_.clear();
}

bool PasswordAutofillManager::FillSuggestionForTest(
    int key,
    const base::string16& username) {
  return FillSuggestion(key, username);
}

bool PasswordAutofillManager::PreviewSuggestionForTest(
    int key,
    const base::string16& username) {
  return PreviewSuggestion(key, username);
}

void PasswordAutofillManager::OnPopupShown() {
}

void PasswordAutofillManager::OnPopupHidden() {
}

void PasswordAutofillManager::DidSelectSuggestion(const base::string16& value,
                                                  int identifier) {
  ClearPreviewedForm();
  bool success = PreviewSuggestion(form_data_key_, value);
  DCHECK(success);
}

void PasswordAutofillManager::DidAcceptSuggestion(const base::string16& value,
                                                  int identifier) {
  bool success = FillSuggestion(form_data_key_, value);
  DCHECK(success);
  autofill_client_->HideAutofillPopup();
}

void PasswordAutofillManager::RemoveSuggestion(const base::string16& value,
                                               int identifier) {
  NOTREACHED();
}

void PasswordAutofillManager::ClearPreviewedForm() {
  password_manager_driver_->ClearPreviewedForm();
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
  if (fill_data.username_field.value == current_username) {
    *password = fill_data.password_field.value;
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
    int key,
    autofill::PasswordFormFillData* found_password) {
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(key);
  if (iter == login_to_password_info_.end())
    return false;

  *found_password = iter->second;
  return true;
}

}  //  namespace password_manager
