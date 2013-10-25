// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTEXT_MENU_H_
#define ASH_SHELL_CONTEXT_MENU_H_

#include "ash/shelf/shelf_alignment_menu.h"
#include "ash/shelf/shelf_types.h"
#include "base/basictypes.h"
#include "ui/base/models/simple_menu_model.h"

namespace aura {
class RootWindow;
}

namespace ash {
namespace shell {

// Context menu for the ash_shell.
class ContextMenu : public ui::SimpleMenuModel,
                    public ui::SimpleMenuModel::Delegate {
 public:
  explicit ContextMenu(aura::Window* root);
  virtual ~ContextMenu();

  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  enum MenuItem {
    MENU_AUTO_HIDE,
    MENU_ALIGNMENT_MENU,
  };

  aura::Window* root_window_;

  ShelfAlignmentMenu alignment_menu_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenu);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_CONTEXT_MENU_H_
