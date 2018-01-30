// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/lifetime/application_lifetime.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

namespace settings {

BrowserLifetimeHandler::BrowserLifetimeHandler() {}

BrowserLifetimeHandler::~BrowserLifetimeHandler() {}

void BrowserLifetimeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("restart",
      base::Bind(&BrowserLifetimeHandler::HandleRestart,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("relaunch",
      base::Bind(&BrowserLifetimeHandler::HandleRelaunch,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback("signOutAndRestart",
      base::Bind(&BrowserLifetimeHandler::HandleSignOutAndRestart,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("factoryReset",
      base::Bind(&BrowserLifetimeHandler::HandleFactoryReset,
                 base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)
}

void BrowserLifetimeHandler::HandleRestart(
    const base::ListValue* args) {
  chrome::AttemptRestart();
}

void BrowserLifetimeHandler::HandleRelaunch(
    const base::ListValue* args) {
  chrome::AttemptRelaunch();
}

#if defined(OS_CHROMEOS)
void BrowserLifetimeHandler::HandleSignOutAndRestart(
    const base::ListValue* args) {
  chrome::AttemptUserExit();
}

void BrowserLifetimeHandler::HandleFactoryReset(
    const base::ListValue* args) {
  const base::Value::ListStorage& args_list = args->GetList();
  CHECK_EQ(1U, args_list.size());
  bool tpm_firmware_update_requested = args_list[0].GetBool();

  if (tpm_firmware_update_requested) {
    chromeos::tpm_firmware_update::ShouldOfferUpdateViaPowerwash(
        base::BindOnce([](bool offer_update) {
          if (!offer_update)
            return;

          PrefService* prefs = g_browser_process->local_state();
          prefs->SetBoolean(prefs::kFactoryResetRequested, true);
          prefs->SetBoolean(prefs::kFactoryResetTPMFirmwareUpdateRequested,
                            true);
          prefs->CommitPendingWrite();
          chrome::AttemptRelaunch();
        }));
    return;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool allow_powerwash = !connector->IsEnterpriseManaged() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
      !user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser();

  if (!allow_powerwash)
    return;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  // Perform sign out. Current chrome process will then terminate, new one will
  // be launched (as if it was a restart).
  chrome::AttemptRelaunch();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
