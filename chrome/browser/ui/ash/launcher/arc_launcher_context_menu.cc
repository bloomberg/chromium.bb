// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"

#include "ash/shelf/shelf_item_types.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/grit/generated_resources.h"

ArcLauncherContextMenu::ArcLauncherContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    ash::Shelf* shelf)
    : LauncherContextMenu(controller, item, shelf) {
  Init();
}

ArcLauncherContextMenu::~ArcLauncherContextMenu() {}

void ArcLauncherContextMenu::Init() {
  const bool app_is_open = controller()->IsOpen(item().id);
  if (!app_is_open) {
    AddItemWithStringId(MENU_OPEN_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
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
