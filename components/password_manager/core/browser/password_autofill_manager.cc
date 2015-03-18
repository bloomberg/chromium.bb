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
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/strings/grit/components_strings.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

// Tests if |username| and |suggestion| are the same. This is different from the
// usual string operator== in that an empty username will only match the
// (non-empty) description of the empty username, used in the suggestions UI.
bool CompareUsernameSuggestion(const base::string16& username,
                               const base::string16& suggestion) {
  if (username.empty()) {
    return suggestion ==
           l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  }
  return username == suggestion;
}

// Returns |username| unless it is empty. For an empty |username| returns a
// localised string saying this username is empty. Use this for displaying the
// usernames to the user.
base::string16 ReplaceEmptyUsername(const base::string16& username) {
  if (username.empty())
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  return username;
}

// Returns the prettified version of |signon_realm| to be displayed on the UI.
base::string16 GetHumanReadableRealm(const std::string& signon_realm) {
  // For Android application realms, remove the hash component. Otherwise, make
  // no changes.
  FacetURI maybe_facet_uri(FacetURI::FromPotentiallyInvalidSpec(signon_realm));
  if (maybe_facet_uri.IsValidAndroidFacetURI())
    return base::UTF8ToUTF16("android://" +
                             maybe_facet_uri.android_package_name() + "/");
  return base::UTF8ToUTF16(signon_realm);
}

// This function attempts to fill |suggestions| and |realms| form |fill_data|
// based on |current_username|. Unless |show_all| is true, it only picks
// suggestions where the username has |current_username| as a prefix.
void GetSuggestions(const autofill::PasswordFormFillData& fill_data,
                    const base::string16& current_username,
                    std::vector<autofill::Suggestion>* suggestions,
                    bool show_all) {
  if (show_all ||
      StartsWith(fill_data.username_field.value, current_username, false)) {
    autofill::Suggestion suggestion(
        ReplaceEmptyUsername(fill_data.username_field.value));
    suggestion.label = GetHumanReadableRealm(fill_data.preferred_realm);
    suggestion.frontend_id = autofill::POPUP_ITEM_ID_PASSWORD_ENTRY;
    suggestions->push_back(suggestion);
  }

  for (const auto& login : fill_data.additional_logins) {
    if (show_all || StartsWith(login.first, current_username, false)) {
      autofill::Suggestion suggestion(ReplaceEmptyUsername(login.first));
      suggestion.label = GetHumanReadableRealm(login.second.realm);
      suggestion.frontend_id = autofill::POPUP_ITEM_ID_PASSWORD_ENTRY;
      suggestions->push_back(suggestion);
    }
  }

  for (const auto& usernames : fill_data.other_possible_usernames) {
    for (size_t i = 0; i < usernames.second.size(); ++i) {
      if (show_all ||
          StartsWith(usernames.second[i], current_username, false)) {
        autofill::Suggestion suggestion(
            ReplaceEmptyUsername(usernames.second[i]));
        suggestion.label = GetHumanReadableRealm(usernames.first.realm);
        suggestion.frontend_id = autofill::POPUP_ITEM_ID_PASSWORD_ENTRY;
        suggestions->push_back(suggestion);
      }
    }
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client)
    : password_manager_driver_(password_manager_driver),
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
    int options,
    const gfx::RectF& bounds) {
  std::vector<autofill::Suggestion> suggestions;
  LoginToPasswordInfoMap::const_iterator fill_data_it =
      login_to_password_info_.find(key);
  if (fill_data_it == login_to_password_info_.end()) {
    // Probably a compromised renderer.
    NOTREACHED();
    return;
  }
  GetSuggestions(fill_data_it->second, typed_username, &suggestions,
                 options & autofill::SHOW_ALL);

  form_data_key_ = key;

  if (suggestions.empty()) {
    autofill_client_->HideAutofillPopup();
    return;
  }

  if (options & autofill::IS_PASSWORD_FIELD) {
    autofill::Suggestion password_field_suggestions(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE));
    password_field_suggestions.frontend_id = autofill::POPUP_ITEM_ID_TITLE;
    suggestions.insert(suggestions.begin(), password_field_suggestions);
  }
  autofill_client_->ShowAutofillPopup(bounds,
                                      text_direction,
                                      suggestions,
                                      weak_ptr_factory_.GetWeakPtr());
}

void PasswordAutofillManager::DidNavigateMainFrame() {
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
  if (CompareUsernameSuggestion(fill_data.username_field.value,
                                current_username)) {
    *password = fill_data.password_field.value;
    return true;
  }

  // Scan additional logins for a match.
  for (autofill::PasswordFormFillData::LoginCollection::const_iterator iter =
           fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end(); ++iter) {
    if (CompareUsernameSuggestion(iter->first, current_username)) {
      *password = iter->second.password;
      return true;
    }
  }

  for (autofill::PasswordFormFillData::UsernamesCollection::const_iterator
           usernames_iter = fill_data.other_possible_usernames.begin();
       usernames_iter != fill_data.other_possible_usernames.end();
       ++usernames_iter) {
    for (size_t i = 0; i < usernames_iter->second.size(); ++i) {
      if (CompareUsernameSuggestion(usernames_iter->second[i],
                                    current_username)) {
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
