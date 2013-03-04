// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_H_

#include "ash/launcher/launcher_delegate.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"

// A menu model that builds the contents of a menu for a launcher item
// containing a list of running applications.
class LauncherApplicationMenuItemModel : public ash::LauncherMenuModel,
                                         public ui::SimpleMenuModel::Delegate {
 public:
  explicit LauncherApplicationMenuItemModel(
      ChromeLauncherAppMenuItems item_list);
  virtual ~LauncherApplicationMenuItemModel();

  // Overridden from ash::LauncherMenuModel:
  virtual bool IsCommandActive(int command_id) const OVERRIDE;

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  void Build();

  // The list of menu items as returned from the launcher controller.
  ChromeLauncherAppMenuItems launcher_items_;

  DISALLOW_COPY_AND_ASSIGN(LauncherApplicationMenuItemModel);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_H_
