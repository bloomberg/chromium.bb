// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ui_base_features.h"

ArcAppContextMenu::ArcAppContextMenu(app_list::AppContextMenuDelegate* delegate,
                                     Profile* profile,
                                     const std::string& app_id,
                                     AppListControllerDelegate* controller)
    : app_list::AppContextMenu(delegate, profile, app_id, controller) {}

ArcAppContextMenu::~ArcAppContextMenu() = default;

void ArcAppContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->set_histogram_name("Apps.ContextMenuExecuteCommand.FromApp");
  BuildMenu(menu_model.get());
  if (!features::IsTouchableAppContextMenuEnabled()) {
    std::move(callback).Run(std::move(menu_model));
    return;
  }
  BuildAppShortcutsMenu(std::move(menu_model), std::move(callback));
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
    AddContextMenuOption(menu_model, LAUNCH_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    if (!features::IsTouchableAppContextMenuEnabled())
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  // Create default items.
  app_list::AppContextMenu::BuildMenu(menu_model);

  if (!features::IsTouchableAppContextMenuEnabled())
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  if (arc_prefs->IsShortcut(app_id()))
    AddContextMenuOption(menu_model, UNINSTALL, IDS_APP_LIST_REMOVE_SHORTCUT);
  else if (!app_info->sticky)
    AddContextMenuOption(menu_model, UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM);

  // App Info item.
  AddContextMenuOption(menu_model, SHOW_APP_INFO,
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
  if (command_id == LAUNCH_NEW) {
    delegate()->ExecuteLaunchCommand(event_flags);
  } else if (command_id == UNINSTALL) {
    arc::ShowArcAppUninstallDialog(profile(), controller(), app_id());
  } else if (command_id == SHOW_APP_INFO) {
    ShowPackageInfo();
  } else if (command_id >= LAUNCH_APP_SHORTCUT_FIRST &&
             command_id <= LAUNCH_APP_SHORTCUT_LAST) {
    ExecuteLaunchAppShortcutCommand(command_id);
  } else {
    app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

void ArcAppContextMenu::BuildAppShortcutsMenu(
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    LOG(ERROR) << "App " << app_id() << " is not available.";
    std::move(callback).Run(std::move(menu_model));
    arc_app_shortcuts_request_.reset();
    return;
  }

  DCHECK(!arc_app_shortcuts_request_);
  // Using base::Unretained(this) here is safe becuase |this| owns
  // |arc_app_shortcuts_request_|. When |this| is deleted,
  // |arc_app_shortcuts_request_| is also deleted, and once that happens,
  // |arc_app_shortcuts_request_| will never run the callback.
  arc_app_shortcuts_request_ =
      std::make_unique<arc::ArcAppShortcutsRequest>(base::BindOnce(
          &ArcAppContextMenu::OnGetAppShortcutItems, base::Unretained(this),
          std::move(menu_model), std::move(callback)));
  arc_app_shortcuts_request_->StartForPackage(app_info->package_name);
}

void ArcAppContextMenu::OnGetAppShortcutItems(
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback,
    std::unique_ptr<arc::ArcAppShortcutItems> app_shortcut_items) {
  app_shortcut_items_ = std::move(app_shortcut_items);
  if (app_shortcut_items_) {
    int command_id = LAUNCH_APP_SHORTCUT_FIRST;
    DCHECK_LT(command_id + app_shortcut_items_->size(),
              LAUNCH_APP_SHORTCUT_LAST);
    for (const auto& item : *app_shortcut_items_)
      menu_model->AddItemWithIcon(command_id++, item.short_label, item.icon);
  }
  std::move(callback).Run(std::move(menu_model));
  arc_app_shortcuts_request_.reset();
}

void ArcAppContextMenu::ExecuteLaunchAppShortcutCommand(int command_id) {
  DCHECK(command_id >= LAUNCH_APP_SHORTCUT_FIRST &&
         command_id <= LAUNCH_APP_SHORTCUT_LAST);
  size_t index = command_id - LAUNCH_APP_SHORTCUT_FIRST;
  DCHECK(app_shortcut_items_);
  DCHECK_LT(index, app_shortcut_items_->size());
  arc::LaunchAppShortcutItem(profile(), app_id(),
                             app_shortcut_items_->at(index).shortcut_id,
                             controller()->GetAppListDisplayId());
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
                           arc::mojom::ShowPackageInfoPage::MAIN,
                           controller()->GetAppListDisplayId()) &&
      !controller()->IsHomeLauncherEnabledInTabletMode()) {
    controller()->DismissView();
  }
}
