// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_H_

#include "ash/shelf/shelf_menu_model.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

class ChromeLauncherAppMenuItem;

// A list of the elements which makes up a simple menu description.
typedef ScopedVector<ChromeLauncherAppMenuItem> ChromeLauncherAppMenuItems;

// A menu model that builds the contents of a menu for a launcher item
// containing a list of running applications.
class LauncherApplicationMenuItemModel : public ash::ShelfMenuModel,
                                         public ui::SimpleMenuModel::Delegate {
 public:
  explicit LauncherApplicationMenuItemModel(
      ChromeLauncherAppMenuItems item_list);
  ~LauncherApplicationMenuItemModel() override;

  // Overridden from ash::ShelfMenuModel:
  bool IsCommandActive(int command_id) const override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void Build();

  // The list of menu items as returned from the launcher controller.
  ChromeLauncherAppMenuItems launcher_items_;

  DISALLOW_COPY_AND_ASSIGN(LauncherApplicationMenuItemModel);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_H_
