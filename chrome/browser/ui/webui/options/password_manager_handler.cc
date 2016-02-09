// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/experiments.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN) && defined(USE_ASH)
#include "chrome/browser/ui/ash/ash_util.h"
#endif

namespace options {

namespace {
// The following constants should be synchronized with the constants in
// chrome/browser/resources/options/password_manager_list.js.
const char kOriginField[] = "origin";
const char kShownUrlField[] = "shownUrl";
const char kIsAndroidUriField[] = "isAndroidUri";
const char kIsSecureField[] = "isSecure";
const char kUsernameField[] = "username";
const char kPasswordField[] = "password";
const char kFederationField[] = "federation";

// Copies from |form| to |entry| the origin, shown origin, whether the origin is
// Android URI, and whether the origin is secure.
void CopyOriginInfoOfPasswordForm(const autofill::PasswordForm& form,
                                  const std::string& languages,
                                  base::DictionaryValue* entry) {
  entry->SetString(
      kOriginField,
      url_formatter::FormatUrl(
          form.origin, languages, url_formatter::kFormatUrlOmitNothing,
          net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  bool is_android_uri = false;
  entry->SetString(kShownUrlField, password_manager::GetShownOrigin(
                                       form, languages, &is_android_uri));
  entry->SetBoolean(kIsAndroidUriField, is_android_uri);
  entry->SetBoolean(kIsSecureField, content::IsOriginSecure(form.origin));
}

}  // namespace

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
      {"autoSigninTitle", IDS_PASSWORDS_AUTO_SIGNIN_TITLE},
      {"autoSigninDescription", IDS_PASSWORDS_AUTO_SIGNIN_DESCRIPTION},
      {"savedPasswordsTitle", IDS_PASSWORD_MANAGER_SHOW_PASSWORDS_TAB_TITLE},
      {"passwordExceptionsTitle", IDS_PASSWORD_MANAGER_EXCEPTIONS_TAB_TITLE},
      {"passwordSearchPlaceholder", IDS_PASSWORDS_PAGE_SEARCH_PASSWORDS},
      {"passwordShowButton", IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON},
      {"passwordHideButton", IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON},
      {"passwordsNoPasswordsDescription",
       IDS_PASSWORDS_PAGE_VIEW_NO_PASSWORDS_DESCRIPTION},
      {"passwordsNoExceptionsDescription",
       IDS_PASSWORDS_PAGE_VIEW_NO_EXCEPTIONS_DESCRIPTION},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));

  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());
  int title_id =
      password_bubble_experiment::IsSmartLockBrandingEnabled(sync_service)
          ? IDS_PASSWORD_MANAGER_SMART_LOCK_FOR_PASSWORDS
          : IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE;
  RegisterTitle(localized_strings, "passwordsPage", title_id);

  localized_strings->SetString("passwordManagerLearnMoreURL",
                               chrome::kPasswordManagerLearnMoreURL);
  localized_strings->SetString(
      "passwordsManagePasswordsLink",
      password_manager::kPasswordManagerAccountDashboardURL);

  std::string management_hostname =
      GURL(password_manager::kPasswordManagerAccountDashboardURL).host();
  base::string16 link_text = base::UTF8ToUTF16(management_hostname);
  size_t offset;
  base::string16 full_text = l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_REMOTE_TEXT, link_text, &offset);

  localized_strings->SetString("passwordsManagePasswordsBeforeLinkText",
                               full_text.substr(0, offset));
  localized_strings->SetString("passwordsManagePasswordsLinkText",
                               full_text.substr(offset, link_text.size()));
  localized_strings->SetString("passwordsManagePasswordsAfterLinkText",
                               full_text.substr(offset + link_text.size()));

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
    const std::string& origin_url,
    const std::string& username,
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
    const std::vector<scoped_ptr<autofill::PasswordForm>>& password_list,
    bool show_passwords) {
  base::ListValue entries;
  languages_ = GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  base::string16 placeholder(base::ASCIIToUTF16("        "));
  for (const auto& saved_password : password_list) {
    scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    CopyOriginInfoOfPasswordForm(*saved_password, languages_, entry.get());

    entry->SetString(kUsernameField, saved_password->username_value);
    if (show_passwords) {
      entry->SetString(kPasswordField, saved_password->password_value);
    } else {
      // Use a placeholder value with the same length as the password.
      entry->SetString(
          kPasswordField,
          base::string16(saved_password->password_value.length(), ' '));
    }
    const GURL& federation_url = saved_password->federation_url;
    if (!federation_url.is_empty()) {
      entry->SetString(
          kFederationField,
          l10n_util::GetStringFUTF16(IDS_PASSWORDS_VIA_FEDERATION,
                                     base::UTF8ToUTF16(federation_url.host())));
    }

    entries.Append(entry.release());
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setSavedPasswordsList",
                                   entries);
}

void PasswordManagerHandler::SetPasswordExceptionList(
    const std::vector<scoped_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  base::ListValue entries;
  for (const auto& exception : password_exception_list) {
    scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    CopyOriginInfoOfPasswordForm(*exception, languages_, entry.get());
    entries.Append(entry.release());
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setPasswordExceptionsList",
                                   entries);
}

}  // namespace options
