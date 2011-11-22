// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

AccountsOptionsHandler::AccountsOptionsHandler() {
}

AccountsOptionsHandler::~AccountsOptionsHandler() {
}

void AccountsOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("whitelistUser",
      base::Bind(&AccountsOptionsHandler::WhitelistUser,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("unwhitelistUser",
      base::Bind(&AccountsOptionsHandler::UnwhitelistUser,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("whitelistExistingUsers",
      base::Bind(&AccountsOptionsHandler::WhitelistExistingUsers,
                 base::Unretained(this)));
}

void AccountsOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "accountsPage",
                IDS_OPTIONS_ACCOUNTS_TAB_LABEL);

  localized_strings->SetString("allow_BWSI", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ALLOW_BWSI_DESCRIPTION));
  localized_strings->SetString("use_whitelist",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USE_WHITELIST_DESCRIPTION));
  localized_strings->SetString("show_user_on_signin",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_SHOW_USER_NAMES_ON_SINGIN_DESCRIPTION));
  localized_strings->SetString("username_edit_hint",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_EDIT_HINT));
  localized_strings->SetString("username_format",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_FORMAT));
  localized_strings->SetString("add_users",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ADD_USERS));
  localized_strings->SetString("owner_only", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_OWNER_ONLY));

  std::string owner;
  CrosSettings::Get()->GetString(kDeviceOwner, &owner);
  localized_strings->SetString("owner_user_id", UTF8ToUTF16(owner));

  localized_strings->SetString("current_user_is_owner",
      UserManager::Get()->current_user_is_owner() ?
      ASCIIToUTF16("true") : ASCIIToUTF16("false"));
  localized_strings->SetString("logged_in_as_guest",
      UserManager::Get()->IsLoggedInAsGuest() ?
      ASCIIToUTF16("true") : ASCIIToUTF16("false"));
  localized_strings->SetString("whitelist_is_managed",
      g_browser_process->browser_policy_connector()->IsEnterpriseManaged() ?
          ASCIIToUTF16("true") : ASCIIToUTF16("false"));
}

void AccountsOptionsHandler::WhitelistUser(const base::ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    return;
  }

  scoped_ptr<base::StringValue> canonical_email(
      base::Value::CreateStringValue(Authenticator::Canonicalize(email)));
  CrosSettings::Get()->AppendToList(kAccountsPrefUsers, canonical_email.get());
}

void AccountsOptionsHandler::UnwhitelistUser(const base::ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    return;
  }

  scoped_ptr<base::StringValue> canonical_email(
      base::Value::CreateStringValue(Authenticator::Canonicalize(email)));
  CrosSettings::Get()->RemoveFromList(kAccountsPrefUsers,
                                      canonical_email.get());
  UserManager::Get()->RemoveUser(email, NULL);
}

void AccountsOptionsHandler::WhitelistExistingUsers(
    const base::ListValue* args) {
  base::ListValue whitelist_users;
  const UserList& users = UserManager::Get()->GetUsers();
  for (UserList::const_iterator it = users.begin(); it < users.end(); ++it) {
    const std::string& email = (*it)->email();
    if (!CrosSettings::Get()->FindEmailInList(kAccountsPrefUsers, email)) {
      base::DictionaryValue* user_dict = new DictionaryValue;
      user_dict->SetString("name", (*it)->GetDisplayName());
      user_dict->SetString("email", email);
      user_dict->SetBoolean("owner", false);

      whitelist_users.Append(user_dict);
    }
  }

  web_ui_->CallJavascriptFunction("AccountsOptions.addUsers", whitelist_users);
}

}  // namespace chromeos
