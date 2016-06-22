// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/grit/generated_resources.h"

ArcAppContextMenu::ArcAppContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller)
    : app_list::AppContextMenu(delegate, profile, app_id, controller) {
}

ArcAppContextMenu::~ArcAppContextMenu() {
}

void ArcAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  if (!controller()->IsAppOpen(app_id())) {
    menu_model->AddItemWithStringId(LAUNCH_NEW,
                                    IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  // Create default items.
  app_list::AppContextMenu::BuildMenu(menu_model);
  if (CanBeUninstalled()) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
    DCHECK(arc_prefs);
    if (arc_prefs->IsShortcut(app_id())) {
      menu_model->AddItemWithStringId(UNINSTALL, IDS_APP_LIST_REMOVE_SHORTCUT);
    } else {
      menu_model->AddItemWithStringId(UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM);
    }
  }
  // App Info item.
  menu_model->AddItemWithStringId(SHOW_APP_INFO,
                                  IDS_APP_CONTEXT_MENU_SHOW_INFO);
}

void ArcAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case LAUNCH_NEW:
      delegate()->ExecuteLaunchCommand(event_flags);
      break;
    case TOGGLE_PIN:
      TogglePin(
          ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id()));
      break;
    case UNINSTALL:
      UninstallPackage();
      break;
    case SHOW_APP_INFO:
      ShowPackageInfo();
      break;
    default:
      app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

void ArcAppContextMenu::UninstallPackage() {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    VLOG(2) << "Package being uninstalled does not exist: " << app_id() << ".";
    return;
  }
  if (app_info->shortcut) {
    // for shortcut we just remove the shortcut instead of the package
    arc_prefs->RemoveApp(app_id());
  } else {
    arc::UninstallPackage(app_info->package_name);
  }
}

void ArcAppContextMenu::ShowPackageInfo() {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    VLOG(2) << "Requesting AppInfo for package that does not exist: "
            << app_id() << ".";
    return;
  }
  if (arc::ShowPackageInfo(app_info->package_name))
    controller()->DismissView();
}

bool ArcAppContextMenu::CanBeUninstalled() const {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  return app_info && app_info->ready && !app_info->sticky;
}
