// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_v2app.h"

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"

ChromeLauncherAppMenuItemV2App::ChromeLauncherAppMenuItemV2App(
    const string16 title,
    const gfx::Image* icon,
    const std::string& app_id,
    ChromeLauncherControllerPerApp* launcher_controller,
    int app_index)
    : ChromeLauncherAppMenuItem(title, icon),
      launcher_controller_(launcher_controller),
      app_id_(app_id),
      app_index_(app_index) {
}

bool ChromeLauncherAppMenuItemV2App::IsEnabled() const {
  return true;
}

void ChromeLauncherAppMenuItemV2App::Execute() {
  // Note: If the application item did go away since the menu was created,
  // The controller will take care of it.
  launcher_controller_->ActivateShellApp(app_id_, app_index_);
}
