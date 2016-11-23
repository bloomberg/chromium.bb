// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h>

#include <algorithm>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/arc/arc_bridge_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Preference for the dictionary of user ids for which cryptohomes have to be
// removed upon browser restart.
const char kArcKioskUsersToRemove[] = "arc-kiosk-users-to-remove";

void ScheduleDelayedCryptohomeRemoval(const cryptohome::Identification& id) {
  PrefService* const local_state = g_browser_process->local_state();
  ListPrefUpdate list_update(local_state, kArcKioskUsersToRemove);

  list_update->AppendString(id.id());
  local_state->CommitPendingWrite();
}

void CancelDelayedCryptohomeRemoval(const cryptohome::Identification& id) {
  PrefService* const local_state = g_browser_process->local_state();
  ListPrefUpdate list_update(local_state, kArcKioskUsersToRemove);
  list_update->Remove(base::StringValue(id.id()), nullptr);
  local_state->CommitPendingWrite();
}

void OnRemoveAppCryptohomeComplete(const cryptohome::Identification& id,
                                   const base::Closure& callback,
                                   bool success,
                                   cryptohome::MountError return_code) {
  if (success) {
    CancelDelayedCryptohomeRemoval(id);
  } else {
    ScheduleDelayedCryptohomeRemoval(id);
    LOG(ERROR) << "Remove one of the cryptohomes failed, return code: "
               << return_code;
  }
  if (!callback.is_null())
    callback.Run();
}

void PerformDelayedCryptohomeRemovals(bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Crypthomed is not available.";
    return;
  }

  PrefService* const local_state = g_browser_process->local_state();
  const base::ListValue* const list =
      local_state->GetList(kArcKioskUsersToRemove);
  for (base::ListValue::const_iterator it = list->begin(); it != list->end();
       ++it) {
    std::string entry;
    if (!(*it)->GetAsString(&entry)) {
      LOG(ERROR) << "List of cryptohome ids is broken";
      continue;
    }
    const cryptohome::Identification cryptohome_id(
        cryptohome::Identification::FromString(entry));
    cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
        cryptohome_id, base::Bind(&OnRemoveAppCryptohomeComplete, cryptohome_id,
                                  base::Closure()));
  }
}

// This class is owned by ChromeBrowserMainPartsChromeos.
static ArcKioskAppManager* g_arc_kiosk_app_manager = nullptr;

}  // namespace

// static
void ArcKioskAppManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kArcKioskUsersToRemove);
}

// static
void ArcKioskAppManager::RemoveObsoleteCryptohomes() {
  chromeos::CryptohomeClient* const client =
      chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
  client->WaitForServiceToBeAvailable(
      base::Bind(&PerformDelayedCryptohomeRemovals));
}

ArcKioskAppManager::ArcKioskApp::ArcKioskApp(const ArcKioskApp& other) =
    default;

ArcKioskAppManager::ArcKioskApp::ArcKioskApp(
    const policy::ArcKioskAppBasicInfo& app_info,
    const AccountId& account_id,
    const std::string& name)
    : app_info_(app_info), account_id_(account_id), name_(name) {}

ArcKioskAppManager::ArcKioskApp::~ArcKioskApp() {}

bool ArcKioskAppManager::ArcKioskApp::operator==(
    const policy::ArcKioskAppBasicInfo& app_info) const {
  return this->app_info_ == app_info;
}

// static
ArcKioskAppManager* ArcKioskAppManager::Get() {
  return g_arc_kiosk_app_manager;
}

ArcKioskAppManager::ArcKioskAppManager()
    : auto_launch_account_id_(EmptyAccountId()) {
  DCHECK(!g_arc_kiosk_app_manager);  // Only one instance is allowed.
  UpdateApps();
  local_accounts_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::Bind(&ArcKioskAppManager::UpdateApps, base::Unretained(this)));
  local_account_auto_login_id_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::Bind(&ArcKioskAppManager::UpdateApps, base::Unretained(this)));
  g_arc_kiosk_app_manager = this;
}

ArcKioskAppManager::~ArcKioskAppManager() {
  local_accounts_subscription_.reset();
  local_account_auto_login_id_subscription_.reset();
  apps_.clear();
  g_arc_kiosk_app_manager = nullptr;
}

const AccountId& ArcKioskAppManager::GetAutoLaunchAccountId() const {
  return auto_launch_account_id_;
}

const ArcKioskAppManager::ArcKioskApp* ArcKioskAppManager::GetAppByAccountId(
    const AccountId& account_id) {
  for (auto& app : GetAllApps())
    if (app.account_id() == account_id)
      return &app;
  return nullptr;
}

void ArcKioskAppManager::AddObserver(ArcKioskAppManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ArcKioskAppManager::RemoveObserver(ArcKioskAppManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ArcKioskAppManager::UpdateApps() {
  // Do not populate ARC kiosk apps if ARC apps can't be run on the device.
  // Apps won't be added to kiosk Apps menu and won't be auto-launched.
  if (!arc::ArcBridgeService::GetEnabled(
          base::CommandLine::ForCurrentProcess())) {
    VLOG(1) << "Device doesn't support ARC apps, don't populate ARC kiosk apps";
    return;
  }

  // Store current apps. We will compare old and new apps to determine which
  // apps are new, and which were deleted.
  ArcKioskApps old_apps(std::move(apps_));

  auto_launch_account_id_.clear();
  std::string auto_login_account_id_from_settings;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id_from_settings);

  // Re-populates |apps_| and reuses existing apps when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->type != policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP)
      continue;

    const AccountId account_id(AccountId::FromUserEmail(it->user_id));

    if (it->account_id == auto_login_account_id_from_settings)
      auto_launch_account_id_ = account_id;

    auto old_it =
        std::find(old_apps.begin(), old_apps.end(), it->arc_kiosk_app_info);
    if (old_it != old_apps.end()) {
      apps_.push_back(std::move(*old_it));
      old_apps.erase(old_it);
    } else {
      apps_.push_back(ArcKioskApp(it->arc_kiosk_app_info, account_id,
                                  it->arc_kiosk_app_info.package_name()));
    }
    CancelDelayedCryptohomeRemoval(cryptohome::Identification(account_id));
  }

  ClearRemovedApps(old_apps);

  for (auto& observer : observers_) {
    observer.OnArcKioskAppsChanged();
  }
}

void ArcKioskAppManager::ClearRemovedApps(
    const std::vector<ArcKioskApp>& old_apps) {
  // Check if currently active user must be deleted.
  bool active_user_to_be_deleted = false;
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (active_user) {
    const AccountId active_account_id = active_user->GetAccountId();
    for (const auto& it : old_apps) {
      if (it.account_id() == active_account_id) {
        active_user_to_be_deleted = true;
        break;
      }
    }
  }

  // Remove cryptohome
  for (auto& entry : old_apps) {
    const cryptohome::Identification cryptohome_id(entry.account_id());
    if (active_user_to_be_deleted) {
      // Schedule cryptohome removal after active user logout.
      ScheduleDelayedCryptohomeRemoval(cryptohome_id);
    } else {
      cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
          cryptohome_id, base::Bind(&OnRemoveAppCryptohomeComplete,
                                    cryptohome_id, base::Closure()));
    }
  }

  if (active_user_to_be_deleted)
    chrome::AttemptUserExit();
}

}  // namespace chromeos
