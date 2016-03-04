// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_

#include "ash/shelf/shelf_alignment_menu.h"
#include "ash/shelf/shelf_item_types.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/models/simple_menu_model.h"

class ChromeLauncherController;

namespace aura {
class Window;
}

namespace extensions {
class ContextMenuMatcher;
}

// Context menu shown for a launcher item.
class LauncherContextMenu : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  // Creates a context menu for the desktop, or shelf |item|, in |root_window|.
  LauncherContextMenu(ChromeLauncherController* controller,
                      const ash::ShelfItem* item,
                      aura::Window* root_window);

  ~LauncherContextMenu() override;

  void Init();

  // ID of the item we're showing the context menu for.
  ash::ShelfID id() const { return item_.id; }

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      LauncherContextMenuTest,
      NewIncognitoWindowMenuIsDisabledWhenIncognitoModeOff);
  FRIEND_TEST_ALL_PREFIXES(
      LauncherContextMenuTest,
      NewWindowMenuIsDisabledWhenIncognitoModeForced);
  FRIEND_TEST_ALL_PREFIXES(
      LauncherContextMenuTest,
      AutoHideOptionInMaximizedMode);

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

  // Does |item_| represent a valid item? See description of constructor for
  // details on why it may not be valid.
  bool is_valid_item() const { return item_.id != 0; }

  ChromeLauncherController* controller_;

  ash::ShelfItem item_;

  ash::ShelfAlignmentMenu shelf_alignment_menu_;

  scoped_ptr<extensions::ContextMenuMatcher> extension_items_;

  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(LauncherContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
