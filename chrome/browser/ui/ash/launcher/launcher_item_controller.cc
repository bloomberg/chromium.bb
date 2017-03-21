// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/extensions/extension_constants.h"

LauncherItemController::LauncherItemController(
    const ash::AppLaunchId& app_launch_id,
    ChromeLauncherController* launcher_controller)
    : app_launch_id_(app_launch_id),
      shelf_id_(0),
      launcher_controller_(launcher_controller),
      locked_(0),
      image_set_by_controller_(false) {}

LauncherItemController::~LauncherItemController() {}

MenuItemList LauncherItemController::GetAppMenuItems(int event_flags) {
  return MenuItemList();
}

AppWindowLauncherItemController*
LauncherItemController::AsAppWindowLauncherItemController() {
  return nullptr;
}
