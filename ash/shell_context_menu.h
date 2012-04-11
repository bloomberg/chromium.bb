// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTEXT_MENU_H_
#define ASH_SHELL_CONTEXT_MENU_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "ui/base/models/simple_menu_model.h"

namespace gfx {
class Point;
class Size;
}

namespace views {
class MenuRunner;
class Widget;
}

namespace ash {
namespace internal {

class ShellContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  ShellContextMenu();
  virtual ~ShellContextMenu();

  // Shows the context menu when right click on background.
  void ShowMenu(views::Widget* widget, const gfx::Point& location);

  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

 private:
  enum MenuItem {
    MENU_CHANGE_WALLPAPER,
  };

  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(ShellContextMenu);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELL_CONTEXT_MENU_H_
