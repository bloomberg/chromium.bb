// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/extensions/extension_constants.h"

LauncherItemController::LauncherItemController(
    Type type,
    const std::string& app_id,
    const std::string& launch_id,
    ChromeLauncherController* launcher_controller)
    : type_(type),
      app_id_(app_id),
      launch_id_(launch_id),
      shelf_id_(0),
      launcher_controller_(launcher_controller),
      locked_(0),
      image_set_by_controller_(false) {}

LauncherItemController::~LauncherItemController() {}

ash::ShelfItemType LauncherItemController::GetShelfItemType() const {
  if (extension_misc::IsImeMenuExtensionId(app_id_))
    return ash::TYPE_IME_MENU;

  switch (type_) {
    case LauncherItemController::TYPE_SHORTCUT:
      return ash::TYPE_APP_SHORTCUT;
    case LauncherItemController::TYPE_APP:
      return ash::TYPE_APP;
    case LauncherItemController::TYPE_APP_PANEL:
      return ash::TYPE_APP_PANEL;
  }
  NOTREACHED();
  return ash::TYPE_APP_SHORTCUT;
}
