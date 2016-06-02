// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

class ChromeLauncherController;

class ArcAppWindowLauncherItemController
    : public AppWindowLauncherItemController {
 public:
  ArcAppWindowLauncherItemController(const std::string& arc_app_id,
                                     ChromeLauncherController* controller);

  ~ArcAppWindowLauncherItemController() override;

  // LauncherItemController overrides:
  base::string16 GetTitle() override;
  ash::ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  ChromeLauncherAppMenuItems GetApplicationList(int event_flags) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppWindowLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
