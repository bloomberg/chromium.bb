// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/passwords_exceptions_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

PasswordsExceptionsHandler::PasswordsExceptionsHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(populater_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(exception_populater_(this)) {
}

PasswordsExceptionsHandler::~PasswordsExceptionsHandler() {
}

void PasswordsExceptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("savedPasswordsExceptionsTitle",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE));
  localized_strings->SetString("passwordsTabTitle",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_SHOW_PASSWORDS_TAB_TITLE));
  localized_strings->SetString("passwordExceptionsTabTitle",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_EXCEPTIONS_TAB_TITLE));
  localized_strings->SetString("passwordsSiteColumn",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN));
  localized_strings->SetString("passwordsUsernameColumn",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN));
  localized_strings->SetString("passwordsRemoveButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_REMOVE_BUTTON));
  localized_strings->SetString("passwordsRemoveAllButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_REMOVE_ALL_BUTTON));
  localized_strings->SetString("passwordsShowButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON));
  localized_strings->SetString("passwordsHideButton",
      l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON));
  localized_strings->SetString("passwordsRemoveAllTitle",
      l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_VIEW_CAPTION_DELETE_ALL_PASSWORDS));
  localized_strings->SetString("passwordsRemoveAllWarning",
      l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_VIEW_TEXT_DELETE_ALL_PASSWORDS));
}

void PasswordsExceptionsHandler::Initialize() {
  // We should not cache dom_ui_->GetProfile(). See crosbug.com/6304.
}

void PasswordsExceptionsHandler::RegisterMessages() {
  DCHECK(dom_ui_);

  dom_ui_->RegisterMessageCallback("loadLists",
      NewCallback(this, &PasswordsExceptionsHandler::LoadLists));
  dom_ui_->RegisterMessageCallback("removeSavedPassword",
      NewCallback(this, &PasswordsExceptionsHandler::RemoveSavedPassword));
  dom_ui_->RegisterMessageCallback("removePasswordException",
      NewCallback(this, &PasswordsExceptionsHandler::RemovePasswordException));
  dom_ui_->RegisterMessageCallback("removeAllSavedPasswords",
      NewCallback(this, &PasswordsExceptionsHandler::RemoveAllSavedPasswords));
  dom_ui_->RegisterMessageCallback("removeAllPasswordExceptions", NewCallback(
      this, &PasswordsExceptionsHandler::RemoveAllPasswordExceptions));
  dom_ui_->RegisterMessageCallback("showSelectedPassword",
      NewCallback(this, &PasswordsExceptionsHandler::ShowSelectedPassword));
}

PasswordStore* PasswordsExceptionsHandler::GetPasswordStore() {
  return dom_ui_->GetProfile()->GetPasswordStore(Profile::EXPLICIT_ACCESS);
}

void PasswordsExceptionsHandler::LoadLists(const ListValue* args) {
  languages_ =
      dom_ui_->GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  populater_.Populate();
  exception_populater_.Populate();
}

void PasswordsExceptionsHandler::RemoveSavedPassword(const ListValue* args) {
  std::string string_value = WideToUTF8(ExtractStringValue(args));
  int index;
  base::StringToInt(string_value, &index);

  GetPasswordStore()->RemoveLogin(*password_list_[index]);
  delete password_list_[index];
  password_list_.erase(password_list_.begin() + index);
  SetPasswordList();
}

void PasswordsExceptionsHandler::RemovePasswordException(
    const ListValue* args) {
  std::string string_value = WideToUTF8(ExtractStringValue(args));
  int index;
  base::StringToInt(string_value, &index);

  GetPasswordStore()->RemoveLogin(*password_exception_list_[index]);
  delete password_exception_list_[index];
  password_exception_list_.erase(password_exception_list_.begin() + index);
  SetPasswordExceptionList();
}

void PasswordsExceptionsHandler::RemoveAllSavedPasswords(
    const ListValue* args) {
  PasswordStore* store = GetPasswordStore();
  for (size_t i = 0; i < password_list_.size(); ++i)
    store->RemoveLogin(*password_list_[i]);
  STLDeleteElements(&password_list_);
  SetPasswordList();
}

void PasswordsExceptionsHandler::RemoveAllPasswordExceptions(
    const ListValue* args) {
  PasswordStore* store = GetPasswordStore();
  for (size_t i = 0; i < password_exception_list_.size(); ++i)
    store->RemoveLogin(*password_exception_list_[i]);
  STLDeleteElements(&password_exception_list_);
  SetPasswordExceptionList();
}

void PasswordsExceptionsHandler::ShowSelectedPassword(const ListValue* args) {
  std::string string_value = WideToUTF8(ExtractStringValue(args));
  int index;
  base::StringToInt(string_value, &index);

  std::string pass = UTF16ToUTF8(password_list_[index]->password_value);
  scoped_ptr<Value> password_string(Value::CreateStringValue(pass));
  dom_ui_->CallJavascriptFunction(
      L"PasswordsExceptions.selectedPasswordCallback", *password_string.get());
}

void PasswordsExceptionsHandler::SetPasswordList() {
  ListValue entries;
  for (size_t i = 0; i < password_list_.size(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue(net::FormatUrl(password_list_[i]->origin,
                                                 languages_)));
    entry->Append(new StringValue(password_list_[i]->username_value));
    entries.Append(entry);
  }

  dom_ui_->CallJavascriptFunction(
      L"PasswordsExceptions.setSavedPasswordsList", entries);
}

void PasswordsExceptionsHandler::SetPasswordExceptionList() {
  ListValue entries;
  for (size_t i = 0; i < password_exception_list_.size(); ++i) {
    entries.Append(new StringValue(
        net::FormatUrl(password_exception_list_[i]->origin, languages_)));
  }

  dom_ui_->CallJavascriptFunction(
      L"PasswordsExceptions.setPasswordExceptionsList", entries);
}

void PasswordsExceptionsHandler::PasswordListPopulater::Populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  pending_login_query_ = store->GetAutofillableLogins(this);
}

void PasswordsExceptionsHandler::PasswordListPopulater::
    OnPasswordStoreRequestDone(int handle,
    const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_list_ = result;
  page_->SetPasswordList();
}

void PasswordsExceptionsHandler::PasswordExceptionListPopulater::Populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  pending_login_query_ = store->GetBlacklistLogins(this);
}

void PasswordsExceptionsHandler::PasswordExceptionListPopulater::
    OnPasswordStoreRequestDone(int handle,
    const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_exception_list_ = result;
  page_->SetPasswordExceptionList();
}
