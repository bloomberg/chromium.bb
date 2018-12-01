// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions.h"

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;
using chromeos::assistant::mojom::AndroidAppInfoPtr;
using chromeos::assistant::mojom::AppStatus;

namespace {

AppStatus GetAndroidAppStatus(const std::string& package_name) {
  auto* prefs = ArcAppListPrefs::Get(ProfileManager::GetActiveUserProfile());
  if (!prefs) {
    LOG(ERROR) << "ArcAppListPrefs is not available.";
    return AppStatus::UNKNOWN;
  }
  std::string app_id = prefs->GetAppIdByPackageName(package_name);

  return app_id.empty() ? AppStatus::UNAVAILABLE : AppStatus::AVAILABLE;
}

}  // namespace

DeviceActions::DeviceActions() {}

DeviceActions::~DeviceActions() = default;

void DeviceActions::SetWifiEnabled(bool enabled) {
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), enabled,
      chromeos::network_handler::ErrorCallback());
}

void DeviceActions::SetBluetoothEnabled(bool enabled) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  // Simply toggle the user pref, which is being observed by ash's bluetooth
  // power controller.
  profile->GetPrefs()->SetBoolean(ash::prefs::kUserBluetoothAdapterEnabled,
                                  enabled);
}

void HandleScreenBrightnessCallback(
    DeviceActions::GetScreenBrightnessLevelCallback callback,
    base::Optional<double> level) {
  if (level.has_value()) {
    std::move(callback).Run(true, level.value() / 100.0);
  } else {
    std::move(callback).Run(false, 0.0);
  }
}

void DeviceActions::GetScreenBrightnessLevel(
    DeviceActions::GetScreenBrightnessLevelCallback callback) {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetScreenBrightnessPercent(
          base::BindOnce(&HandleScreenBrightnessCallback, std::move(callback)));
}

void DeviceActions::SetScreenBrightnessLevel(double level, bool gradual) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(level * 100);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_GRADUAL
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetScreenBrightness(request);
}

void DeviceActions::SetNightLightEnabled(bool enabled) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  // Simply toggle the user pref, which is being observed by ash's night
  // light controller.
  profile->GetPrefs()->SetBoolean(ash::prefs::kNightLightEnabled, enabled);
}

void DeviceActions::OpenAndroidApp(AndroidAppInfoPtr app_info,
                                   OpenAndroidAppCallback callback) {
  app_info->status = GetAndroidAppStatus(app_info->package_name);

  if (app_info->status != AppStatus::AVAILABLE) {
    std::move(callback).Run(false);
    return;
  }

  auto* helper = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper(),
      HandleIntent);
  if (!helper) {
    LOG(ERROR) << "Android container is not running.";
    std::move(callback).Run(false);
    return;
  }

  arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
  activity->package_name = app_info->package_name;
  auto intent = arc::mojom::IntentInfo::New();
  if (!app_info->intent.empty()) {
    intent->data = app_info->intent;
  }
  helper->HandleIntent(std::move(intent), std::move(activity));

  std::move(callback).Run(true);
}

void DeviceActions::VerifyAndroidApp(
    std::vector<chromeos::assistant::mojom::AndroidAppInfoPtr> apps_info,
    VerifyAndroidAppCallback callback) {
  for (const auto& app_info : apps_info) {
    app_info->status = GetAndroidAppStatus(app_info->package_name);
  }
  std::move(callback).Run(std::move(apps_info));
}
