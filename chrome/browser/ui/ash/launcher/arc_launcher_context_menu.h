// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_LAUNCHER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_LAUNCHER_CONTEXT_MENU_H_

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

namespace ash {
struct ShelfItem;
class WmShelf;
}

// Class for context menu which is shown for Arc app in the shelf.
class ArcLauncherContextMenu : public LauncherContextMenu {
 public:
  ArcLauncherContextMenu(ChromeLauncherControllerImpl* controller,
                         const ash::ShelfItem* item,
                         ash::WmShelf* wm_shelf);
  ~ArcLauncherContextMenu() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  void Init();

  DISALLOW_COPY_AND_ASSIGN(ArcLauncherContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_LAUNCHER_CONTEXT_MENU_H_
