// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/window_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace wm {

// TODO(beng): replace many of these functions with the corewm versions.
void ActivateWindow(aura::Window* window) {
  views::corewm::ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  views::corewm::DeactivateWindow(window);
}

bool IsActiveWindow(aura::Window* window) {
  return views::corewm::IsActiveWindow(window);
}

aura::Window* GetActiveWindow() {
  return aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return views::corewm::GetActivatableWindow(window);
}

bool CanActivateWindow(aura::Window* window) {
  return views::corewm::CanActivateWindow(window);
}

bool CanMaximizeWindow(const aura::Window* window) {
  return window->GetProperty(aura::client::kCanMaximizeKey);
}

bool CanMinimizeWindow(const aura::Window* window) {
  internal::RootWindowController* controller =
      internal::RootWindowController::ForWindow(window);
  if (!controller)
    return false;
  aura::Window* lockscreen = controller->GetContainer(
      internal::kShellWindowId_LockScreenContainersContainer);
  if (lockscreen->Contains(window))
    return false;

  return true;
}

bool CanResizeWindow(const aura::Window* window) {
  return window->GetProperty(aura::client::kCanResizeKey);
}

bool CanSnapWindow(aura::Window* window) {
  if (!CanResizeWindow(window))
    return false;
  // If a window has a maximum size defined, snapping may make it too big.
  return window->delegate() ? window->delegate()->GetMaximumSize().IsEmpty() :
                              true;
}

bool IsWindowNormal(const aura::Window* window) {
  return IsWindowStateNormal(window->GetProperty(aura::client::kShowStateKey));
}

bool IsWindowStateNormal(ui::WindowShowState state) {
  return state == ui::SHOW_STATE_NORMAL || state == ui::SHOW_STATE_DEFAULT;
}

bool IsWindowMaximized(const aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

bool IsWindowMinimized(const aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED;
}

bool IsWindowFullscreen(const aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_FULLSCREEN;
}

void MaximizeWindow(aura::Window* window) {
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void MinimizeWindow(aura::Window* window) {
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
}

void RestoreWindow(aura::Window* window) {
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

void ToggleMaximizedWindow(aura::Window* window) {
  if (ash::wm::IsWindowMaximized(window))
    ash::wm::RestoreWindow(window);
  else if (ash::wm::CanMaximizeWindow(window))
    ash::wm::MaximizeWindow(window);
}

void CenterWindow(aura::Window* window) {
  const gfx::Display display =
      Shell::GetScreen()->GetDisplayNearestWindow(window);
  gfx::Rect center = display.work_area();
  center.ClampToCenteredSize(window->bounds().size());
  window->SetBoundsInScreen(center, display);
}

bool IsWindowPositionManaged(const aura::Window* window) {
  return window->GetProperty(ash::internal::kWindowPositionManagedKey);
}

void SetWindowPositionManaged(aura::Window* window, bool managed) {
  window->SetProperty(ash::internal::kWindowPositionManagedKey, managed);
}

bool HasUserChangedWindowPositionOrSize(const aura::Window* window) {
  return window->GetProperty(
      ash::internal::kUserChangedWindowPositionOrSizeKey);
}

void SetUserHasChangedWindowPositionOrSize(aura::Window* window, bool changed) {
  window->SetProperty(ash::internal::kUserChangedWindowPositionOrSizeKey,
                      changed);
}

const gfx::Rect* GetPreAutoManageWindowBounds(const aura::Window* window) {
  return window->GetProperty(ash::internal::kPreAutoManagedWindowBoundsKey);
}

void SetPreAutoManageWindowBounds(aura::Window* window,
                                const gfx::Rect& bounds) {
  window->SetProperty(ash::internal::kPreAutoManagedWindowBoundsKey,
                      new gfx::Rect(bounds));
}

void AdjustBoundsToEnsureMinimumWindowVisibility(const gfx::Rect& visible_area,
                                                 gfx::Rect* bounds) {
  AdjustBoundsToEnsureWindowVisibility(
      visible_area, kMinimumOnScreenArea, kMinimumOnScreenArea, bounds);
}

void AdjustBoundsToEnsureWindowVisibility(const gfx::Rect& visible_area,
                                          int min_width,
                                          int min_height,
                                          gfx::Rect* bounds) {
  bounds->set_width(std::min(bounds->width(), visible_area.width()));
  bounds->set_height(std::min(bounds->height(), visible_area.height()));

  min_width = std::min(min_width, visible_area.width());
  min_height = std::min(min_height, visible_area.height());

  if (bounds->x() + min_width > visible_area.right()) {
    bounds->set_x(visible_area.right() - min_width);
  } else if (bounds->right() - min_width < 0) {
    bounds->set_x(min_width - bounds->width());
  }
  if (bounds->y() + min_height > visible_area.bottom()) {
    bounds->set_y(visible_area.bottom() - min_height);
  } else if (bounds->bottom() - min_height < 0) {
    bounds->set_y(min_height - bounds->height());
  }
}

bool MoveWindowToEventRoot(aura::Window* window, const ui::Event& event) {
  views::View* target = static_cast<views::View*>(event.target());
  if (!target)
    return false;
  aura::RootWindow* target_root =
      target->GetWidget()->GetNativeView()->GetRootWindow();
  if (!target_root || target_root == window->GetRootWindow())
    return false;
  aura::Window* window_container =
      ash::Shell::GetContainer(target_root, window->parent()->id());
  // Move the window to the target launcher.
  window_container->AddChild(window);
  return true;
}

}  // namespace wm
}  // namespace ash
