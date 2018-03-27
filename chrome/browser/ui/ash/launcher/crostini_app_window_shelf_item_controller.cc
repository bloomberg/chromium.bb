// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_item_controller.h"

#include <utility>

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"

CrostiniAppWindowShelfItemController::CrostiniAppWindowShelfItemController(
    const ash::ShelfID& shelf_id)
    : AppWindowLauncherItemController(shelf_id) {}

CrostiniAppWindowShelfItemController::~CrostiniAppWindowShelfItemController() {}

void CrostiniAppWindowShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  if (window_count()) {
    AppWindowLauncherItemController::ItemSelected(std::move(event), display_id,
                                                  source, std::move(callback));
    return;
  }

  NOTIMPLEMENTED()
      << "TODO(crbug.com/824549): Make Crostini apps able to be pinned";
}
