// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
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
  AppContextMenu::BuildMenu(menu_model);
  if (CanBeUninstalled()) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model->AddItemWithStringId(UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM);
  }
}

bool ArcAppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == LAUNCH_NEW) {
    ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id());
    return app_info && app_info->ready;
  }
  return AppContextMenu::IsCommandIdEnabled(command_id);
}

void ArcAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case LAUNCH_NEW:
      delegate()->ExecuteLaunchCommand(event_flags);
      break;
    case UNINSTALL:
      UninstallPackage();
      break;
    default:
      AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

void ArcAppContextMenu::UninstallPackage() {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    LOG(ERROR) << "Package being uninstalling does not exist";
    return;
  }
  arc::UninstallPackage(app_info->package_name);
}

bool ArcAppContextMenu::CanBeUninstalled() const {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  return app_info && app_info->ready && !app_info->sticky;
}
