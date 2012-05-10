// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
#define ASH_WM_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/launcher_alignment_menu.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/basictypes.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

// Context menu for the launcher.
class ASH_EXPORT LauncherContextMenu : public ui::SimpleMenuModel,
                                       public ui::SimpleMenuModel::Delegate {
 public:
  LauncherContextMenu();
  virtual ~LauncherContextMenu();

  // Returns true if the auto-hide menu item is checked.
  static bool IsAutoHideMenuHideChecked();

  // Returns the toggled state of the auto-hide behavior.
  static ShelfAutoHideBehavior GetToggledAutoHideBehavior();

  // Returns the resource id for the auto-hide menu.
  static int GetAutoHideResourceStringId();

  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  enum MenuItem {
    MENU_AUTO_HIDE,
    MENU_ALIGNMENT_MENU,
  };

  LauncherAlignmentMenu alignment_menu_;

  DISALLOW_COPY_AND_ASSIGN(LauncherContextMenu);
};

}  // namespace ash

#endif  // ASH_WM_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
