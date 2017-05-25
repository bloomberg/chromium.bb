// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/desktop_shell_launcher_context_menu.h"

DesktopShellLauncherContextMenu::DesktopShellLauncherContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    ash::Shelf* shelf)
    : LauncherContextMenu(controller, item, shelf) {
  Init();
}

DesktopShellLauncherContextMenu::~DesktopShellLauncherContextMenu() {}

void DesktopShellLauncherContextMenu::Init() {
  AddShelfOptionsMenu();
}
