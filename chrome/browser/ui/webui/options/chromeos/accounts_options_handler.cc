// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Adds specified user to the whitelist. Returns false if that user is already
// in the whitelist.
bool WhitelistUser(const std::string& username) {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (cros_settings->FindEmailInList(kAccountsPrefUsers, username))
    return false;
  base::StringValue username_value(username);
  cros_settings->AppendToList(kAccountsPrefUsers, &username_value);
  return true;
}

}  // namespace

namespace options {

AccountsOptionsHandler::AccountsOptionsHandler() {
}

AccountsOptionsHandler::~AccountsOptionsHandler() {
}

void AccountsOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("whitelistUser",
      base::Bind(&AccountsOptionsHandler::HandleWhitelistUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("unwhitelistUser",
      base::Bind(&AccountsOptionsHandler::HandleUnwhitelistUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("whitelistExistingUsers",
      base::Bind(&AccountsOptionsHandler::HandleWhitelistExistingUsers,
                 base::Unretained(this)));
}

void AccountsOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "accountsPage",
                IDS_OPTIONS_ACCOUNTS_TAB_LABEL);

  localized_strings->SetString("allow_BWSI", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ALLOW_BWSI_DESCRIPTION));
  localized_strings->SetString("use_whitelist", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USE_WHITELIST_DESCRIPTION));
  localized_strings->SetString("show_user_on_signin", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_SHOW_USER_NAMES_ON_SINGIN_DESCRIPTION));
  localized_strings->SetString("username_edit_hint", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_EDIT_HINT));
  localized_strings->SetString("username_format", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_FORMAT));
  localized_strings->SetString("add_users", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ADD_USERS));
  localized_strings->SetString("owner_only", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_OWNER_ONLY));

  localized_strings->SetBoolean("whitelist_is_managed",
      g_browser_process->browser_policy_connector()->IsEnterpriseManaged());

  AddAccountUITweaksLocalizedValues(localized_strings);
}

void AccountsOptionsHandler::HandleWhitelistUser(const base::ListValue* args) {
  std::string typed_email;
  std::string name;
  if (!args->GetString(0, &typed_email) ||
      !args->GetString(1, &name)) {
    return;
  }

  WhitelistUser(gaia::CanonicalizeEmail(typed_email));
}

void AccountsOptionsHandler::HandleUnwhitelistUser(
    const base::ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    return;
  }

  base::StringValue canonical_email(gaia::CanonicalizeEmail(email));
  CrosSettings::Get()->RemoveFromList(kAccountsPrefUsers, &canonical_email);
  UserManager::Get()->RemoveUser(email, NULL);
}

void AccountsOptionsHandler::HandleWhitelistExistingUsers(
    const base::ListValue* args) {
  DCHECK(args && args->empty());

  // Creates one list to set. This is needed because user white list update is
  // asynchronous and sequential. Before previous write comes back, cached list
  // is stale and should not be used for appending. See http://crbug.com/127215
  scoped_ptr<base::ListValue> new_list;

  CrosSettings* cros_settings = CrosSettings::Get();
  const base::ListValue* existing = NULL;
  if (cros_settings->GetList(kAccountsPrefUsers, &existing) && existing)
    new_list.reset(existing->DeepCopy());
  else
    new_list.reset(new base::ListValue);

  const UserList& users = UserManager::Get()->GetUsers();
  for (UserList::const_iterator it = users.begin(); it < users.end(); ++it)
    new_list->AppendIfNotPresent(new base::StringValue((*it)->email()));

  cros_settings->Set(kAccountsPrefUsers, *new_list.get());
}

}  // namespace options
}  // namespace chromeos
