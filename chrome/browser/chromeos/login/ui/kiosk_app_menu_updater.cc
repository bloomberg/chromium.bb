// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/kiosk_app_menu_updater.h"

#include <utility>

#include "ash/public/interfaces/kiosk_app_info.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "content/public/browser/notification_service.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

KioskAppMenuUpdater::KioskAppMenuUpdater()
    : kiosk_observer_(this), arc_kiosk_observer_(this) {
  kiosk_observer_.Add(KioskAppManager::Get());
  arc_kiosk_observer_.Add(ArcKioskAppManager::Get());
}

KioskAppMenuUpdater::~KioskAppMenuUpdater() = default;

void KioskAppMenuUpdater::OnKioskAppDataChanged(const std::string& app_id) {
  SendKioskApps();
}

void KioskAppMenuUpdater::OnKioskAppDataLoadFailure(const std::string& app_id) {
  SendKioskApps();
}

void KioskAppMenuUpdater::OnKioskAppsSettingsChanged() {
  SendKioskApps();
}

void KioskAppMenuUpdater::OnArcKioskAppsChanged() {
  SendKioskApps();
}

void KioskAppMenuUpdater::OnKioskAppsSet(bool success) {
  if (!success)
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_APPS_LOADED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void KioskAppMenuUpdater::SendKioskApps() {
  if (!LoginScreenClient::HasInstance())
    return;

  std::vector<ash::mojom::KioskAppInfoPtr> output;

  std::vector<KioskAppManager::App> apps;
  KioskAppManager::Get()->GetApps(&apps);
  for (const auto& app : apps) {
    auto mojo_app = ash::mojom::KioskAppInfo::New();
    mojo_app->identifier = ash::mojom::KioskAppIdentifier::New();
    mojo_app->identifier->set_app_id(app.app_id);
    mojo_app->name = base::UTF8ToUTF16(app.name);
    if (app.icon.isNull()) {
      mojo_app->icon = *ui::ResourceBundle::GetSharedInstance()
                            .GetImageNamed(IDR_APP_DEFAULT_ICON)
                            .ToImageSkia();
    } else {
      mojo_app->icon = gfx::ImageSkia(app.icon);
    }
    output.push_back(std::move(mojo_app));
  }

  std::vector<ArcKioskAppData*> arc_apps;
  ArcKioskAppManager::Get()->GetAllApps(&arc_apps);
  for (ArcKioskAppData* app : arc_apps) {
    auto mojo_app = ash::mojom::KioskAppInfo::New();
    mojo_app->identifier = ash::mojom::KioskAppIdentifier::New();
    mojo_app->identifier->set_account_id(app->account_id());
    mojo_app->name = base::UTF8ToUTF16(app->name());
    if (app->icon().isNull()) {
      mojo_app->icon = *ui::ResourceBundle::GetSharedInstance()
                            .GetImageNamed(IDR_APP_DEFAULT_ICON)
                            .ToImageSkia();
    } else {
      mojo_app->icon = gfx::ImageSkia(app->icon());
    }
    output.push_back(std::move(mojo_app));
  }
  LoginScreenClient::Get()->login_screen()->SetKioskApps(
      std::move(output), base::BindOnce(&KioskAppMenuUpdater::OnKioskAppsSet,
                                        weak_factory_.GetWeakPtr()));

  KioskAppLaunchError::Error error = KioskAppLaunchError::Get();
  if (error == KioskAppLaunchError::NONE)
    return;

  // Clear any old pending Kiosk launch errors
  KioskAppLaunchError::RecordMetricAndClear();

  LoginScreenClient::Get()->login_screen()->ShowKioskAppError(
      KioskAppLaunchError::GetErrorMessage(error));
}

}  // namespace chromeos
