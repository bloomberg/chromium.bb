// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#if defined(OS_WIN) && defined(USE_ASH)
#include "chrome/browser/ui/ash/ash_util.h"
#endif
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/experiments.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

PasswordManagerHandler::PasswordManagerHandler()
    : password_manager_presenter_(this) {}

PasswordManagerHandler::~PasswordManagerHandler() {}

Profile* PasswordManagerHandler::GetProfile() {
  return Profile::FromWebUI(web_ui());
}

#if !defined(OS_ANDROID)
gfx::NativeWindow PasswordManagerHandler::GetNativeWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}
#endif

void PasswordManagerHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static const OptionsStringResource resources[] = {
    { "autoSigninTitle",
      IDS_PASSWORDS_AUTO_SIGNIN_TITLE },
    { "autoSigninDescription",
      IDS_PASSWORDS_AUTO_SIGNIN_DESCRIPTION },
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
  localized_strings->SetString("passwordsManagePasswordsLink",
                               chrome::kPasswordManagerAccountDashboardURL);

  std::vector<base::string16> pieces;
  base::SplitStringDontTrim(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_REMOTE_TEXT),
      '|',  // separator
      &pieces);
  DCHECK_EQ(3U, pieces.size());
  localized_strings->SetString("passwordsManagePasswordsBeforeLinkText",
                               pieces[0]);
  localized_strings->SetString("passwordsManagePasswordsLinkText", pieces[1]);
  localized_strings->SetString("passwordsManagePasswordsAfterLinkText",
                               pieces[2]);

  bool disable_show_passwords = false;

#if defined(OS_WIN) && defined(USE_ASH)
  // We disable the ability to show passwords when running in Windows Metro
  // interface.  This is because we cannot pop native Win32 dialogs from the
  // Metro process.
  // TODO(wfh): Revisit this if Metro usage grows.
  if (chrome::IsNativeWindowInAsh(GetNativeWindow()))
    disable_show_passwords = true;
#endif

  localized_strings->SetBoolean("disableShowPasswords", disable_show_passwords);
  localized_strings->SetBoolean(
      "enableCredentialManagerAPI",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCredentialManagerAPI));
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

void PasswordManagerHandler::InitializePage() {
  base::FundamentalValue visible(
      password_manager::ManageAccountLinkExperimentEnabled());
  web_ui()->CallJavascriptFunction(
      "PasswordManager.setManageAccountLinkVisibility", visible);
}

void PasswordManagerHandler::HandleRemoveSavedPassword(
    const base::ListValue* args) {
  std::string string_value = base::UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0) {
    password_manager_presenter_.RemoveSavedPassword(static_cast<size_t>(index));
  }
}

void PasswordManagerHandler::HandleRemovePasswordException(
    const base::ListValue* args) {
  std::string string_value = base::UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0) {
    password_manager_presenter_.RemovePasswordException(
        static_cast<size_t>(index));
  }
}

void PasswordManagerHandler::HandleRequestShowPassword(
    const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index))
    NOTREACHED();

  password_manager_presenter_.RequestShowPassword(static_cast<size_t>(index));
}

void PasswordManagerHandler::ShowPassword(
    size_t index,
    const base::string16& password_value) {
  // Call back the front end to reveal the password.
  web_ui()->CallJavascriptFunction(
      "PasswordManager.showPassword",
      base::FundamentalValue(static_cast<int>(index)),
      base::StringValue(password_value));
}

void PasswordManagerHandler::HandleUpdatePasswordLists(
    const base::ListValue* args) {
  password_manager_presenter_.UpdatePasswordLists();
}

void PasswordManagerHandler::SetPasswordList(
    const ScopedVector<autofill::PasswordForm>& password_list,
    bool show_passwords) {
  base::ListValue entries;
  languages_ = GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  base::string16 placeholder(base::ASCIIToUTF16("        "));
  for (size_t i = 0; i < password_list.size(); ++i) {
    base::ListValue* entry = new base::ListValue();
    entry->AppendString(GetHumanReadableOrigin(*password_list[i], languages_));
    entry->AppendString(password_list[i]->username_value);
    if (show_passwords) {
      entry->AppendString(password_list[i]->password_value);
    } else {
      // Use a placeholder value with the same length as the password.
      entry->AppendString(
          base::string16(password_list[i]->password_value.length(), ' '));
    }
    entries.Append(entry);
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setSavedPasswordsList",
                                   entries);
}

void PasswordManagerHandler::SetPasswordExceptionList(
    const ScopedVector<autofill::PasswordForm>& password_exception_list) {
  base::ListValue entries;
  for (size_t i = 0; i < password_exception_list.size(); ++i) {
    entries.Append(new base::StringValue(
        net::FormatUrl(password_exception_list[i]->origin, languages_)));
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setPasswordExceptionsList",
                                   entries);
}

}  // namespace options
