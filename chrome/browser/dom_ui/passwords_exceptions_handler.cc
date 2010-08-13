// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/passwords_exceptions_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

PasswordsExceptionsHandler::PasswordsExceptionsHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(populater_(this)) {
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
  localized_strings->SetString("exceptionsTabTitle",
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
  profile_ = dom_ui_->GetProfile();
  populater_.Populate();
}

void PasswordsExceptionsHandler::RegisterMessages() {
}

PasswordStore* PasswordsExceptionsHandler::GetPasswordStore() {
  return profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS);
}

void PasswordsExceptionsHandler::SetPasswordList(
    const std::vector<webkit_glue::PasswordForm*>& result) {
  ListValue autofillableLogins;
  std::wstring languages =
      UTF8ToWide(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  password_list_ = result;
  for (size_t i = 0; i < result.size(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue(
        WideToUTF8(net::FormatUrl(result[i]->origin, languages))));
    entry->Append(new StringValue(
        UTF16ToUTF8(result[i]->username_value)));
    autofillableLogins.Append(entry);
  }

  dom_ui_->CallJavascriptFunction(
      L"PasswordsExceptions.setAutofillableLogins", autofillableLogins);
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
  page_->SetPasswordList(result);
}
