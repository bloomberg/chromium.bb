// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_SHELF_AUTO_HIDE_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_SHELF_AUTO_HIDE_MENU_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/models/simple_menu_model.h"

class ChromeLauncherDelegate;

// Submenu of LauncherContextMenu for choosing the visibility of the shelf.
class ShelfAutoHideMenu : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  explicit ShelfAutoHideMenu(ChromeLauncherDelegate* delegate);
  virtual ~ShelfAutoHideMenu();

  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  // Menu command id needs to be unique. So starting at 1000 to avoid command
  // id conflicts with parent LauncherContextMenu.
  enum MenuItem {
    MENU_AUTO_HIDE_DEFAULT = 1000,
    MENU_AUTO_HIDE_ALWAYS,
    MENU_AUTO_HIDE_NEVER,
  };

  ChromeLauncherDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShelfAutoHideMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_SHELF_AUTO_HIDE_MENU_H_
