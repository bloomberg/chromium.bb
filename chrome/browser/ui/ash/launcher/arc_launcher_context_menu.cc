// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"

#include "ash/common/shelf/shelf_item_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"
#include "chrome/grit/generated_resources.h"

ArcLauncherContextMenu::ArcLauncherContextMenu(
    ChromeLauncherControllerImpl* controller,
    const ash::ShelfItem* item,
    ash::WmShelf* wm_shelf)
    : LauncherContextMenu(controller, item, wm_shelf) {
  Init();
}

ArcLauncherContextMenu::~ArcLauncherContextMenu() {}

void ArcLauncherContextMenu::Init() {
  const ArcAppListPrefs* arc_list_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_list_prefs);

  const arc::ArcAppShelfId& app_id = arc::ArcAppShelfId::FromString(
      controller()->GetAppIDForShelfID(item().id));
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_list_prefs->GetApp(app_id.app_id());
  if (!app_info && !app_id.has_shelf_group_id()) {
    NOTREACHED();
    return;
  }

  const bool app_is_open = controller()->IsOpen(item().id);
  if (!app_is_open) {
    DCHECK(app_info->launchable);
    AddItemWithStringId(MENU_OPEN_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (!app_id.has_shelf_group_id() && app_info->launchable)
    AddPinMenu();

  if (app_is_open)
    AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddShelfOptionsMenu();
}

bool ArcLauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == MENU_OPEN_NEW)
    return true;
  return LauncherContextMenu::IsCommandIdEnabled(command_id);
}
