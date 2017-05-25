// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_

#include "ash/public/cpp/shelf_item.h"
#include "ash/shelf/shelf_alignment_menu.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

class ChromeLauncherController;

namespace ash {
class Shelf;
}

// Base class for context menu which is shown for a regular extension item in
// the shelf, or for an ARC app item in the shelf, or shown when right click
// on desktop shell.
class LauncherContextMenu : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  // Menu item command ids, used by subclasses and tests.
  enum MenuItem {
    MENU_OPEN_NEW,
    MENU_CLOSE,
    MENU_PIN,
    LAUNCH_TYPE_PINNED_TAB,
    LAUNCH_TYPE_REGULAR_TAB,
    LAUNCH_TYPE_FULLSCREEN,
    LAUNCH_TYPE_WINDOW,
    MENU_AUTO_HIDE,
    MENU_NEW_WINDOW,
    MENU_NEW_INCOGNITO_WINDOW,
    MENU_ALIGNMENT_MENU,
    MENU_CHANGE_WALLPAPER,
    MENU_ITEM_COUNT
  };

  ~LauncherContextMenu() override;

  // Static function to create contextmenu instance.
  static LauncherContextMenu* Create(ChromeLauncherController* controller,
                                     const ash::ShelfItem* item,
                                     ash::Shelf* shelf);

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 protected:
  LauncherContextMenu(ChromeLauncherController* controller,
                      const ash::ShelfItem* item,
                      ash::Shelf* shelf);
  ChromeLauncherController* controller() const { return controller_; }

  const ash::ShelfItem& item() const { return item_; }

  // Add menu item for pin/unpin.
  void AddPinMenu();

  // Add common shelf options items, e.g. autohide, alignment, and wallpaper.
  void AddShelfOptionsMenu();

  // Helper method to execute common commands. Returns true if handled.
  bool ExecuteCommonCommand(int command_id, int event_flags);

 private:
  ChromeLauncherController* controller_;

  ash::ShelfItem item_;

  ash::ShelfAlignmentMenu shelf_alignment_menu_;

  ash::Shelf* shelf_;

  DISALLOW_COPY_AND_ASSIGN(LauncherContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
