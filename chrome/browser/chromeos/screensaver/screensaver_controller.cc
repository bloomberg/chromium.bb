// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/screensaver/screensaver_controller.h"

#include "ash/screensaver/screensaver_view.h"
#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_service.h"

namespace {

const int kScreensaverTimeoutMinutes = 2;

ExtensionService* GetDefaultExtensionService() {
  Profile* default_profile = ProfileManager::GetDefaultProfile();
  DCHECK(default_profile);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(default_profile)->extension_service();
  DCHECK(service);

  return service;
}

// Find the screensaver extension for the given service, excluding
// the one with exclude_id.
std::string FindScreensaverExtension(ExtensionService* service,
                                     const std::string& exclude_id) {
  const ExtensionSet* extensions = service->extensions();
  if (!extensions)
    return std::string();

  for (ExtensionSet::const_iterator it = extensions->begin();
      it != extensions->end();
      ++it) {
    const extensions::Extension* extension = *it;
    if (extension &&
        extension->id() != exclude_id &&
        extension->HasAPIPermission(extensions::APIPermission::kScreensaver)) {
      return extension->id();
    }
  }

  return std::string();
}

void UninstallPreviousScreensavers(const extensions::Extension* current) {
  Profile* default_profile = ProfileManager::GetDefaultProfile();
  DCHECK(default_profile);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(default_profile)->extension_service();
  DCHECK(service);

  std::string screensaver_id;
  while ((screensaver_id = FindScreensaverExtension(service, current->id()))
         != "") {
    string16 error;
    if (!service->UninstallExtension(screensaver_id, false, &error))
      LOG(ERROR)
          << "Couldn't uninstall previous screensaver extension with id: "
          << screensaver_id << " \nError: " << error;
  }
}

}  // namespace

namespace chromeos {

ScreensaverController::ScreensaverController()
    : threshold_(base::TimeDelta::FromMinutes(kScreensaverTimeoutMinutes)),
      weak_ptr_factory_(this) {
  // Register for extension changes.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::NotificationService::AllSources());

  std::string screensaver_extension_id =
      FindScreensaverExtension(GetDefaultExtensionService(), std::string());
  if (!screensaver_extension_id.empty())
    SetupScreensaver(screensaver_extension_id);
}

ScreensaverController::~ScreensaverController() {
  TeardownScreensaver();
}

void ScreensaverController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const extensions::Extension* extension =
          content::Details<const extensions::InstalledExtensionInfo>(details)->
              extension;
      // Uninstall any previously installed screensaver extensions if a new
      // screensaver extension is installed.
      if (extension->HasAPIPermission(extensions::APIPermission::kScreensaver))
        UninstallPreviousScreensavers(extension);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* extension =
          content::Details<const extensions::Extension>(details).ptr();
      if (extension->HasAPIPermission(extensions::APIPermission::kScreensaver))
        SetupScreensaver(extension->id());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::Extension* extension =
          content::Details<extensions::UnloadedExtensionInfo>(
              details)->extension;
      if (extension->id() == screensaver_extension_id_)
        TeardownScreensaver();
      break;
    }
  }
}

void ScreensaverController::IdleNotify(int64 threshold) {
  ExtensionService* service = GetDefaultExtensionService();
  const extensions::Extension* screensaver_extension =
      service->GetExtensionById(screensaver_extension_id_,
                                ExtensionService::INCLUDE_ENABLED);
  ash::ShowScreensaver(screensaver_extension->GetFullLaunchURL());

  if (!ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this))
    ash::Shell::GetInstance()->user_activity_detector()->AddObserver(this);
}

void ScreensaverController::OnUserActivity() {
  // We don't want to handle further user notifications; we'll either login
  // the user and close out or or at least close the screensaver.
  if (ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this))
    ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);
  ash::CloseScreensaver();

  RequestNextIdleNotification();
}

void ScreensaverController::SetupScreensaver(
    const std::string& screensaver_extension_id) {
  screensaver_extension_id_ = screensaver_extension_id;

  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (!power_manager->HasObserver(this))
    power_manager->AddObserver(this);

  RequestNextIdleNotification();
}

void ScreensaverController::TeardownScreensaver() {
  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager && power_manager->HasObserver(this))
    power_manager->RemoveObserver(this);

  if (ash::Shell::GetInstance() &&
      ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this)) {
    ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);
  }

  ash::CloseScreensaver();
  screensaver_extension_id_ = "";
}

void ScreensaverController::RequestNextIdleNotification() {
  DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestIdleNotification(threshold_.InMilliseconds());
}

}  // namespace chromeos
