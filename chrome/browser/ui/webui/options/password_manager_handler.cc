// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

PasswordManagerHandler::PasswordManagerHandler()
    : password_manager_presenter_(this) {}

PasswordManagerHandler::~PasswordManagerHandler() {}

Profile* PasswordManagerHandler::GetProfile() {
  return Profile::FromWebUI(web_ui());
}

void PasswordManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static const OptionsStringResource resources[] = {
    { "savedPasswordsTitle",
      IDS_PASSWORDS_SHOW_PASSWORDS_TAB_TITLE },
    { "passwordExceptionsTitle",
      IDS_PASSWORDS_EXCEPTIONS_TAB_TITLE },
    { "passwordSearchPlaceholder",
      IDS_PASSWORDS_PAGE_SEARCH_PASSWORDS },
    { "passwordShowButton",
      IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON },
    { "passwordHideButton",
      IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON },
    { "passwordsNoPasswordsDescription",
      IDS_PASSWORDS_PAGE_VIEW_NO_PASSWORDS_DESCRIPTION },
    { "passwordsNoExceptionsDescription",
      IDS_PASSWORDS_PAGE_VIEW_NO_EXCEPTIONS_DESCRIPTION },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "passwordsPage",
                IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE);

  localized_strings->SetString("passwordManagerLearnMoreURL",
                               chrome::kPasswordManagerLearnMoreURL);
}

void PasswordManagerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updatePasswordLists",
      base::Bind(&PasswordManagerHandler::HandleUpdatePasswordLists,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeSavedPassword",
      base::Bind(&PasswordManagerHandler::HandleRemoveSavedPassword,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removePasswordException",
      base::Bind(&PasswordManagerHandler::HandleRemovePasswordException,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestShowPassword",
      base::Bind(&PasswordManagerHandler::HandleRequestShowPassword,
                 base::Unretained(this)));
}

void PasswordManagerHandler::InitializeHandler() {
  password_manager_presenter_.Initialize();
}

void PasswordManagerHandler::HandleRemoveSavedPassword(const ListValue* args) {
  std::string string_value = UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0) {
    password_manager_presenter_.RemoveSavedPassword(static_cast<size_t>(index));
  }
}

void PasswordManagerHandler::HandleRemovePasswordException(
    const ListValue* args) {
  std::string string_value = UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0) {
    password_manager_presenter_.RemovePasswordException(
        static_cast<size_t>(index));
  }
}

void PasswordManagerHandler::HandleRequestShowPassword(const ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index))
    NOTREACHED();

  password_manager_presenter_.RequestShowPassword(static_cast<size_t>(index));
}

void PasswordManagerHandler::ShowPassword(size_t index,
                                          const string16& password_value) {
  // Call back the front end to reveal the password.
  web_ui()->CallJavascriptFunction(
      "PasswordManager.showPassword",
      base::FundamentalValue(static_cast<int>(index)),
      StringValue(password_value));
}

void PasswordManagerHandler::HandleUpdatePasswordLists(const ListValue* args) {
  password_manager_presenter_.UpdatePasswordLists();
}

void PasswordManagerHandler::SetPasswordList(
    const ScopedVector<autofill::PasswordForm>& password_list,
    bool show_passwords) {
  ListValue entries;
  languages_ = GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  string16 placeholder(ASCIIToUTF16("        "));
  for (size_t i = 0; i < password_list.size(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue(net::FormatUrl(password_list[i]->origin,
                                                 languages_)));
    entry->Append(new StringValue(password_list[i]->username_value));
    if (show_passwords) {
      entry->Append(new StringValue(password_list[i]->password_value));
    } else {
      // Use a placeholder value with the same length as the password.
      entry->Append(new StringValue(
          string16(password_list[i]->password_value.length(), ' ')));
    }
    entries.Append(entry);
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setSavedPasswordsList",
                                   entries);
}

void PasswordManagerHandler::SetPasswordExceptionList(
    const ScopedVector<autofill::PasswordForm>& password_exception_list) {
  ListValue entries;
  for (size_t i = 0; i < password_exception_list.size(); ++i) {
    entries.Append(new StringValue(
        net::FormatUrl(password_exception_list[i]->origin, languages_)));
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setPasswordExceptionsList",
                                   entries);
}

}  // namespace options
