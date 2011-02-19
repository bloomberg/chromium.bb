// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/options/password_manager_handler.h"

#include "base/callback.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

PasswordManagerHandler::PasswordManagerHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(populater_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(exception_populater_(this)) {
}

PasswordManagerHandler::~PasswordManagerHandler() {
}

void PasswordManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "passwordsPage",
                IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE);
  localized_strings->SetString("savedPasswordsTitle",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_SHOW_PASSWORDS_TAB_TITLE));
  localized_strings->SetString("passwordExceptionsTitle",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_EXCEPTIONS_TAB_TITLE));
  localized_strings->SetString("passwordShowButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON));
  localized_strings->SetString("passwordHideButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON));
  localized_strings->SetString("passwordsSiteColumn",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN));
  localized_strings->SetString("passwordsUsernameColumn",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN));
  localized_strings->SetString("passwordsRemoveButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_REMOVE_BUTTON));
  localized_strings->SetString("passwordsRemoveAllButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_REMOVE_ALL_BUTTON));
  localized_strings->SetString("passwordsRemoveAllTitle",
      l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_VIEW_CAPTION_DELETE_ALL_PASSWORDS));
  localized_strings->SetString("passwordsRemoveAllWarning",
      l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_VIEW_TEXT_DELETE_ALL_PASSWORDS));
}

void PasswordManagerHandler::Initialize() {
  // We should not cache web_ui_->GetProfile(). See crosbug.com/6304.
}

void PasswordManagerHandler::RegisterMessages() {
  DCHECK(web_ui_);

  web_ui_->RegisterMessageCallback("updatePasswordLists",
      NewCallback(this, &PasswordManagerHandler::UpdatePasswordLists));
  web_ui_->RegisterMessageCallback("removeSavedPassword",
      NewCallback(this, &PasswordManagerHandler::RemoveSavedPassword));
  web_ui_->RegisterMessageCallback("removePasswordException",
      NewCallback(this, &PasswordManagerHandler::RemovePasswordException));
  web_ui_->RegisterMessageCallback("removeAllSavedPasswords",
      NewCallback(this, &PasswordManagerHandler::RemoveAllSavedPasswords));
  web_ui_->RegisterMessageCallback("removeAllPasswordExceptions", NewCallback(
      this, &PasswordManagerHandler::RemoveAllPasswordExceptions));
}

PasswordStore* PasswordManagerHandler::GetPasswordStore() {
  return web_ui_->GetProfile()->GetPasswordStore(Profile::EXPLICIT_ACCESS);
}

void PasswordManagerHandler::UpdatePasswordLists(const ListValue* args) {
  languages_ =
      web_ui_->GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  populater_.Populate();
  exception_populater_.Populate();
}

void PasswordManagerHandler::RemoveSavedPassword(const ListValue* args) {
  std::string string_value = WideToUTF8(ExtractStringValue(args));
  int index;
  base::StringToInt(string_value, &index);

  GetPasswordStore()->RemoveLogin(*password_list_[index]);
  delete password_list_[index];
  password_list_.erase(password_list_.begin() + index);
  SetPasswordList();
}

void PasswordManagerHandler::RemovePasswordException(
    const ListValue* args) {
  std::string string_value = WideToUTF8(ExtractStringValue(args));
  int index;
  base::StringToInt(string_value, &index);

  GetPasswordStore()->RemoveLogin(*password_exception_list_[index]);
  delete password_exception_list_[index];
  password_exception_list_.erase(password_exception_list_.begin() + index);
  SetPasswordExceptionList();
}

void PasswordManagerHandler::RemoveAllSavedPasswords(
    const ListValue* args) {
  PasswordStore* store = GetPasswordStore();
  for (size_t i = 0; i < password_list_.size(); ++i)
    store->RemoveLogin(*password_list_[i]);
  STLDeleteElements(&password_list_);
  SetPasswordList();
}

void PasswordManagerHandler::RemoveAllPasswordExceptions(
    const ListValue* args) {
  PasswordStore* store = GetPasswordStore();
  for (size_t i = 0; i < password_exception_list_.size(); ++i)
    store->RemoveLogin(*password_exception_list_[i]);
  STLDeleteElements(&password_exception_list_);
  SetPasswordExceptionList();
}

void PasswordManagerHandler::SetPasswordList() {
  ListValue entries;
  for (size_t i = 0; i < password_list_.size(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue(net::FormatUrl(password_list_[i]->origin,
                                                 languages_)));
    entry->Append(new StringValue(password_list_[i]->username_value));
    entry->Append(new StringValue(password_list_[i]->password_value));
    entries.Append(entry);
  }

  web_ui_->CallJavascriptFunction(
      L"PasswordManager.setSavedPasswordsList", entries);
}

void PasswordManagerHandler::SetPasswordExceptionList() {
  ListValue entries;
  for (size_t i = 0; i < password_exception_list_.size(); ++i) {
    entries.Append(new StringValue(
        net::FormatUrl(password_exception_list_[i]->origin, languages_)));
  }

  web_ui_->CallJavascriptFunction(
      L"PasswordManager.setPasswordExceptionsList", entries);
}

PasswordManagerHandler::ListPopulater::ListPopulater(
    PasswordManagerHandler* page) : page_(page),
                                    pending_login_query_(0) {
}

PasswordManagerHandler::ListPopulater::~ListPopulater() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store)
    store->CancelLoginsQuery(pending_login_query_);
}

PasswordManagerHandler::PasswordListPopulater::PasswordListPopulater(
    PasswordManagerHandler* page) : ListPopulater(page) {
}

void PasswordManagerHandler::PasswordListPopulater::Populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL)
    pending_login_query_ = store->GetAutofillableLogins(this);
  else
    LOG(ERROR) << "No password store! Cannot display passwords.";
}

void PasswordManagerHandler::PasswordListPopulater::
    OnPasswordStoreRequestDone(int handle,
    const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_list_ = result;
  page_->SetPasswordList();
}

PasswordManagerHandler::PasswordExceptionListPopulater::
    PasswordExceptionListPopulater(PasswordManagerHandler* page)
        : ListPopulater(page) {
}

void PasswordManagerHandler::PasswordExceptionListPopulater::Populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL)
    pending_login_query_ = store->GetBlacklistLogins(this);
  else
    LOG(ERROR) << "No password store! Cannot display exceptions.";
}

void PasswordManagerHandler::PasswordExceptionListPopulater::
    OnPasswordStoreRequestDone(int handle,
    const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_exception_list_ = result;
  page_->SetPasswordExceptionList();
}
