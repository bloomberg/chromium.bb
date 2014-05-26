// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"

#include "ash/shell.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "extensions/common/extension.h"
#include "ui/app_list/views/app_list_view.h"

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

gfx::Rect AppListControllerDelegateAsh::GetAppListBounds() {
  app_list::AppListView* app_list_view =
      ash::Shell::GetInstance()->GetAppListView();
  if (app_list_view)
    return app_list_view->GetBoundsInScreen();
  return gfx::Rect();
}

gfx::ImageSkia AppListControllerDelegateAsh::GetWindowIcon() {
  return gfx::ImageSkia();
}

bool AppListControllerDelegateAsh::IsAppPinned(
    const std::string& extension_id) {
  return ChromeLauncherController::instance()->IsAppPinned(extension_id);
}

void AppListControllerDelegateAsh::PinApp(const std::string& extension_id) {
  ChromeLauncherController::instance()->PinAppWithID(extension_id);
}

void AppListControllerDelegateAsh::UnpinApp(const std::string& extension_id) {
  ChromeLauncherController::instance()->UnpinAppWithID(extension_id);
}

AppListControllerDelegate::Pinnable
    AppListControllerDelegateAsh::GetPinnable() {
  return ChromeLauncherController::instance()->CanPin() ? PIN_EDITABLE :
      PIN_FIXED;
}

void AppListControllerDelegateAsh::OnShowChildDialog() {
  app_list::AppListView* app_list_view =
      ash::Shell::GetInstance()->GetAppListView();
  if (app_list_view)
    app_list_view->SetAppListOverlayVisible(true);
}

void AppListControllerDelegateAsh::OnCloseChildDialog() {
  app_list::AppListView* app_list_view =
      ash::Shell::GetInstance()->GetAppListView();
  if (app_list_view)
    app_list_view->SetAppListOverlayVisible(false);
}

bool AppListControllerDelegateAsh::CanDoCreateShortcutsFlow() {
  return false;
}

void AppListControllerDelegateAsh::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  NOTREACHED();
}

void AppListControllerDelegateAsh::CreateNewWindow(Profile* profile,
                                                   bool incognito) {
  if (incognito)
    ChromeLauncherController::instance()->CreateNewIncognitoWindow();
  else
    ChromeLauncherController::instance()->CreateNewWindow();
}

void AppListControllerDelegateAsh::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  ChromeLauncherController::instance()->ActivateApp(
      extension->id(),
      AppListSourceToLaunchSource(source),
      event_flags);

  DismissView();
}

void AppListControllerDelegateAsh::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  ChromeLauncherController::instance()->LaunchApp(
      extension->id(),
      AppListSourceToLaunchSource(source),
      event_flags);
  DismissView();
}

void AppListControllerDelegateAsh::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  // Ash doesn't have profile switching.
  NOTREACHED();
}

bool AppListControllerDelegateAsh::ShouldShowUserIcon() {
  return false;
}

ash::LaunchSource AppListControllerDelegateAsh::AppListSourceToLaunchSource(
    AppListSource source) {
  switch (source) {
    case LAUNCH_FROM_APP_LIST:
      return ash::LAUNCH_FROM_APP_LIST;
    case LAUNCH_FROM_APP_LIST_SEARCH:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
    default:
      return ash::LAUNCH_FROM_UNKNOWN;
  }
}
