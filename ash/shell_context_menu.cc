// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_context_menu.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

ShellContextMenu::ShellContextMenu() {
}

ShellContextMenu::~ShellContextMenu() {
}

void ShellContextMenu::ShowMenu(views::Widget* widget,
                                const gfx::Point& location) {
  ui::SimpleMenuModel menu_model(this);
  menu_model.AddItem(MENU_CHANGE_WALLPAPER,
      l10n_util::GetStringUTF16(IDS_AURA_SET_DESKTOP_WALLPAPER));
  views::MenuModelAdapter menu_model_adapter(&menu_model);
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));
  if (menu_runner_->RunMenuAt(
          widget, NULL, gfx::Rect(location, gfx::Size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS |
          views::MenuRunner::CONTEXT_MENU) == views::MenuRunner::MENU_DELETED)
    return;
}

bool ShellContextMenu::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ShellContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_CHANGE_WALLPAPER: {
      return Shell::GetInstance()->user_wallpaper_delegate()->
          CanOpenSetWallpaperPage();
    }
    default:
      return true;
  }
}

void ShellContextMenu::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_CHANGE_WALLPAPER: {
      Shell::GetInstance()->user_wallpaper_delegate()->
          OpenSetWallpaperPage();
      break;
    }
  }
}

bool ShellContextMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

}  // namespace internal
}  // namespace ash
