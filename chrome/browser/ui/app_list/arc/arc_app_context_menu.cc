// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"
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
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    LOG(ERROR) << "App " << app_id() << " is not available.";
    return;
  }

  if (!controller()->IsAppOpen(app_id())) {
    menu_model->AddItemWithStringId(LAUNCH_NEW,
                                    IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  // Create default items.
  app_list::AppContextMenu::BuildMenu(menu_model);

  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  if (arc_prefs->IsShortcut(app_id()))
    menu_model->AddItemWithStringId(UNINSTALL, IDS_APP_LIST_REMOVE_SHORTCUT);
  else if (!app_info->sticky)
    menu_model->AddItemWithStringId(UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM);

  // App Info item.
  menu_model->AddItemWithStringId(SHOW_APP_INFO,
                                  IDS_APP_CONTEXT_MENU_SHOW_INFO);
}

bool ArcAppContextMenu::IsCommandIdEnabled(int command_id) const {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());

  switch (command_id) {
    case UNINSTALL:
      return app_info &&
          !app_info->sticky &&
          (app_info->ready || app_info->shortcut);
    case SHOW_APP_INFO:
      return app_info && app_info->ready;
    default:
      return app_list::AppContextMenu::IsCommandIdEnabled(command_id);
  }

  return false;
}

void ArcAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case LAUNCH_NEW:
      delegate()->ExecuteLaunchCommand(event_flags);
      break;
    case UNINSTALL:
      arc::ShowArcAppUninstallDialog(profile(), controller(), app_id());
      break;
    case SHOW_APP_INFO:
      ShowPackageInfo();
      break;
    default:
      app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
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
  if (arc::ShowPackageInfo(app_info->package_name,
                           arc::mojom::ShowPackageInfoPage::MAIN)) {
    controller()->DismissView();
  }
}
