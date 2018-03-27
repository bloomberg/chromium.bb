// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

class CrostiniAppWindow;

// Shelf item delegate for Crostini app windows.
class CrostiniAppWindowShelfItemController
    : public AppWindowLauncherItemController {
 public:
  explicit CrostiniAppWindowShelfItemController(const ash::ShelfID& shelf_id);

  ~CrostiniAppWindowShelfItemController() override;

  // AppWindowLauncherItemController overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;

 private:
  // Update the shelf item's icon for the active window.
  void UpdateIcon(CrostiniAppWindow* crostini_app_window);

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppWindowShelfItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_
