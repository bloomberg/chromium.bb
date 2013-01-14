// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"

#include "ash/shell.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

AppListControllerDelegateAsh::AppListControllerDelegateAsh() {}

AppListControllerDelegateAsh::~AppListControllerDelegateAsh() {}

void AppListControllerDelegateAsh::DismissView() {
  DCHECK(ash::Shell::HasInstance());
  if (ash::Shell::GetInstance()->GetAppListTargetVisibility())
    ash::Shell::GetInstance()->ToggleAppList(NULL);
}

gfx::NativeWindow AppListControllerDelegateAsh::GetAppListWindow() {
  DCHECK(ash::Shell::HasInstance());
  return ash::Shell::GetInstance()->GetAppListWindow();
}

bool AppListControllerDelegateAsh::IsAppPinned(
    const std::string& extension_id) {
  return ChromeLauncherController::instance()->IsAppPinned(extension_id);
}

void AppListControllerDelegateAsh::PinApp(const std::string& extension_id) {
  ChromeLauncherController::instance()->PinAppWithID(extension_id);
}

void AppListControllerDelegateAsh::UnpinApp(const std::string& extension_id) {
  ChromeLauncherController::instance()->UnpinAppsWithID(extension_id);
}

bool AppListControllerDelegateAsh::CanPin() {
  return ChromeLauncherController::instance()->CanPin();
}

bool AppListControllerDelegateAsh::CanShowCreateShortcutsDialog() {
  return false;
}

void AppListControllerDelegateAsh::CreateNewWindow(bool incognito) {
  if (incognito)
    ChromeLauncherController::instance()->CreateNewIncognitoWindow();
  else
    ChromeLauncherController::instance()->CreateNewWindow();
}

void AppListControllerDelegateAsh::ActivateApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  ChromeLauncherController::instance()->ActivateApp(extension->id(),
                                                    event_flags);
  DismissView();
}

void AppListControllerDelegateAsh::LaunchApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  ChromeLauncherController::instance()->LaunchApp(extension->id(),
                                                  event_flags);
  DismissView();
}

namespace chrome {

#if defined(OS_CHROMEOS)
void ShowAppList() {
  ash::Shell::GetInstance()->ToggleAppList(NULL);
}
#endif

}  // namespace chrome
