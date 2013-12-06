// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_v2app.h"

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

ChromeLauncherAppMenuItemV2App::ChromeLauncherAppMenuItemV2App(
    const base::string16 title,
    const gfx::Image* icon,
    const std::string& app_id,
    ChromeLauncherController* launcher_controller,
    int app_index,
    bool has_leading_separator)
    : ChromeLauncherAppMenuItem(title, icon, has_leading_separator),
      launcher_controller_(launcher_controller),
      app_id_(app_id),
      app_index_(app_index) {
}

bool ChromeLauncherAppMenuItemV2App::IsEnabled() const {
  return true;
}

void ChromeLauncherAppMenuItemV2App::Execute(int event_flags) {
  // Note: At this time there is only a single app running at any point. as
  // such we will never come here with usable |event_flags|. If that ever
  // changes we should add some special close code here.
  // Note: If the application item did go away since the menu was created,
  // The controller will take care of it.
  launcher_controller_->ActivateShellApp(app_id_, app_index_);
}
