// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Adds specified user to the whitelist. Returns false if that user is already
// in the whitelist.
bool WhitelistUser(OwnerSettingsServiceChromeOS* service,
                   const std::string& username) {
  if (CrosSettings::Get()->FindEmailInList(kAccountsPrefUsers, username, NULL))
    return false;
  if (service) {
    base::Value username_value(username);
    service->AppendToList(kAccountsPrefUsers, username_value);
  }
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
  web_ui()->RegisterMessageCallback("updateWhitelist",
      base::Bind(&AccountsOptionsHandler::HandleUpdateWhitelist,
                 base::Unretained(this)));
}

void AccountsOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "accountsPage",
                IDS_OPTIONS_ACCOUNTS_TAB_LABEL);

  localized_strings->SetString("allow_BWSI", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ALLOW_BWSI_DESCRIPTION));
  localized_strings->SetString(
      "allow_supervised_users",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ACCOUNTS_ENABLE_SUPERVISED_USERS));
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

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  localized_strings->SetBoolean("whitelist_is_managed",
                                connector->IsEnterpriseManaged());

  AddAccountUITweaksLocalizedValues(localized_strings,
                                    Profile::FromWebUI(web_ui()));
}

void AccountsOptionsHandler::HandleWhitelistUser(const base::ListValue* args) {
  std::string typed_email;
  std::string name;
  if (!args->GetString(0, &typed_email) ||
      !args->GetString(1, &name)) {
    return;
  }

  if (OwnerSettingsServiceChromeOS* service =
          OwnerSettingsServiceChromeOS::FromWebUI(web_ui())) {
    WhitelistUser(service, gaia::CanonicalizeEmail(typed_email));
  }
}

void AccountsOptionsHandler::HandleUnwhitelistUser(
    const base::ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    return;
  }

  ProfileMetrics::LogProfileDeleteUser(ProfileMetrics::DELETE_PROFILE_SETTINGS);

  base::Value canonical_email(gaia::CanonicalizeEmail(email));
  if (OwnerSettingsServiceChromeOS* service =
          OwnerSettingsServiceChromeOS::FromWebUI(web_ui())) {
    service->RemoveFromList(kAccountsPrefUsers, canonical_email);
  }
  user_manager::UserManager::Get()->RemoveUser(AccountId::FromUserEmail(email),
                                               nullptr);
}

void AccountsOptionsHandler::HandleUpdateWhitelist(
    const base::ListValue* args) {
  DCHECK(args && args->empty());

  // Creates one list to set. This is needed because user white list update is
  // asynchronous and sequential. Before previous write comes back, cached list
  // is stale and should not be used for appending. See http://crbug.com/127215
  std::unique_ptr<base::ListValue> new_list;

  CrosSettings* cros_settings = CrosSettings::Get();
  const base::ListValue* existing = NULL;
  if (cros_settings->GetList(kAccountsPrefUsers, &existing) && existing)
    new_list.reset(existing->DeepCopy());
  else
    new_list.reset(new base::ListValue);

  // Remove all supervised users. On the next step only supervised users present
  // on the device will be added back. Thus not present SU are removed.
  // No need to remove usual users as they can simply login back.
  for (size_t i = 0; i < new_list->GetSize(); ++i) {
    std::string whitelisted_user;
    new_list->GetString(i, &whitelisted_user);
    if (user_manager::UserManager::Get()->IsSupervisedAccountId(
            AccountId::FromUserEmail(whitelisted_user))) {
      new_list->Remove(i, NULL);
      --i;
    }
  }

  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  for (const auto* user : users) {
    new_list->AppendIfNotPresent(
        base::MakeUnique<base::Value>(user->GetAccountId().GetUserEmail()));
  }

  if (OwnerSettingsServiceChromeOS* service =
          OwnerSettingsServiceChromeOS::FromWebUI(web_ui())) {
    service->Set(kAccountsPrefUsers, *new_list.get());
  }
}

}  // namespace options
}  // namespace chromeos
