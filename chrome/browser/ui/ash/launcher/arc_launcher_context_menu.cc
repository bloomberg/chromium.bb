// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_item.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ui_base_features.h"

ArcLauncherContextMenu::ArcLauncherContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : LauncherContextMenu(controller, item, display_id) {}

ArcLauncherContextMenu::~ArcLauncherContextMenu() = default;

void ArcLauncherContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu(menu_model.get());
  std::move(callback).Run(std::move(menu_model));
}

void ArcLauncherContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  const ArcAppListPrefs* arc_list_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_list_prefs);

  const arc::ArcAppShelfId& app_id =
      arc::ArcAppShelfId::FromString(item().id.app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_list_prefs->GetApp(app_id.app_id());
  if (!app_info && !app_id.has_shelf_group_id()) {
    NOTREACHED();
    return;
  }

  const bool app_is_open = controller()->IsOpen(item().id);
  if (!app_is_open) {
    DCHECK(app_info->launchable);
    AddContextMenuOption(menu_model, MENU_OPEN_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    if (!features::IsTouchableAppContextMenuEnabled())
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (!app_id.has_shelf_group_id() && app_info->launchable)
    AddPinMenu(menu_model);

  if (app_is_open) {
    AddContextMenuOption(menu_model, MENU_CLOSE,
                         IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  }
  if (!features::IsTouchableAppContextMenuEnabled())
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
}
