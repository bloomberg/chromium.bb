// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_filter.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {
namespace internal {

namespace {

// Size of the grid when a grid is enabled.
const int kGridSize = 16;

}  // namespace

WorkspaceController::WorkspaceController(aura::Window* viewport)
    : viewport_(viewport),
      workspace_manager_(new WorkspaceManager(viewport)),
      layout_manager_(NULL),
      event_filter_(NULL) {
  aura::RootWindow* root_window = viewport->GetRootWindow();
  event_filter_ = new WorkspaceEventFilter(viewport);
  viewport->SetEventFilter(event_filter_);
  layout_manager_ = new WorkspaceLayoutManager(
      root_window, workspace_manager_.get());
  viewport->SetLayoutManager(layout_manager_);
  root_window->AddObserver(this);
  SetGridSize(kGridSize);
}

WorkspaceController::~WorkspaceController() {
  viewport_->GetRootWindow()->RemoveObserver(this);
  // WorkspaceLayoutManager may attempt to access state from us. Destroy it now.
  if (viewport_->layout_manager() == layout_manager_)
    viewport_->SetLayoutManager(NULL);
}

void WorkspaceController::ToggleOverview() {
  workspace_manager_->SetOverview(!workspace_manager_->is_overview());
}

void WorkspaceController::ShowMenu(views::Widget* widget,
                                   const gfx::Point& location) {
#if !defined(OS_MACOSX)
  ui::SimpleMenuModel menu_model(this);
  // This is just for testing and will be ripped out before we ship, so none of
  // the strings are localized.
  menu_model.AddCheckItem(MENU_SNAP_TO_GRID,
                          ASCIIToUTF16("Snap to grid"));
  menu_model.AddItem(MENU_CHANGE_WALLPAPER,
                     l10n_util::GetStringUTF16(IDS_AURA_SET_DESKTOP_WALLPAPER));
  views::MenuModelAdapter menu_model_adapter(&menu_model);
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));
  if (menu_runner_->RunMenuAt(
          widget, NULL, gfx::Rect(location, gfx::Size()),
          views::MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
#endif  // !defined(OS_MACOSX)
}

void WorkspaceController::SetGridSize(int grid_size) {
  workspace_manager_->set_grid_size(grid_size);
  event_filter_->set_grid_size(grid_size);
}

void WorkspaceController::OnWindowPropertyChanged(aura::Window* window,
                                                  const void* key,
                                                  intptr_t old) {
  if (key == aura::client::kRootWindowActiveWindowKey)
    workspace_manager_->SetActiveWorkspaceByWindow(wm::GetActiveWindow());
}

bool WorkspaceController::IsCommandIdChecked(int command_id) const {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_SNAP_TO_GRID:
      return workspace_manager_->grid_size() != 0;

    default:
      break;
  }
  return false;
}

bool WorkspaceController::IsCommandIdEnabled(int command_id) const {
  return true;
}

void WorkspaceController::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_SNAP_TO_GRID: {
      int size = workspace_manager_->grid_size() == 0 ? kGridSize : 0;
      SetGridSize(size);
      if (!size)
        return;
      for (size_t i = 0; i < viewport_->children().size(); ++i) {
        aura::Window* window = viewport_->children()[i];
        if (!wm::IsWindowMaximized(window) && !wm::IsWindowFullscreen(window)) {
          window->SetBounds(workspace_manager_->AlignBoundsToGrid(
                                window->GetTargetBounds()));
        }
      }
      break;
    }
    case MENU_CHANGE_WALLPAPER: {
      Shell::GetInstance()->user_wallpaper_delegate()->
          OpenSetWallpaperPage();
      break;
    }
  }
}

bool WorkspaceController::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

}  // namespace internal
}  // namespace ash
