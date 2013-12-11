// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
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

bool IsWindowMinimized(aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED;
}

void CenterWindow(aura::Window* window) {
  wm::WindowState* window_state = wm::GetWindowState(window);
  if (!window_state->IsNormalShowState())
    return;
  const gfx::Display display =
      Shell::GetScreen()->GetDisplayNearestWindow(window);
  gfx::Rect center = display.work_area();
  gfx::Size size = window->bounds().size();
  if (window_state->IsSnapped()) {
    if (window_state->HasRestoreBounds())
      size = window_state->GetRestoreBoundsInScreen().size();
    center.ClampToCenteredSize(size);
    window_state->SetRestoreBoundsInScreen(center);
    window_state->Restore();
  } else {
    center = ScreenAsh::ConvertRectFromScreen(window->parent(),
        center);
    center.ClampToCenteredSize(size);
    window->SetBounds(center);
  }
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
  if (bounds->y() < 0)
    bounds->set_y(0);
}

bool MoveWindowToEventRoot(aura::Window* window, const ui::Event& event) {
  views::View* target = static_cast<views::View*>(event.target());
  if (!target)
    return false;
  aura::Window* target_root =
      target->GetWidget()->GetNativeView()->GetRootWindow();
  if (!target_root || target_root == window->GetRootWindow())
    return false;
  aura::Window* window_container =
      ash::Shell::GetContainer(target_root, window->parent()->id());
  // Move the window to the target launcher.
  window_container->AddChild(window);
  return true;
}

void ReparentChildWithTransientChildren(aura::Window* child,
                                        aura::Window* old_parent,
                                        aura::Window* new_parent) {
  if (child->parent() == old_parent)
    new_parent->AddChild(child);
  ReparentTransientChildrenOfChild(child, old_parent, new_parent);
}

void ReparentTransientChildrenOfChild(aura::Window* child,
                                      aura::Window* old_parent,
                                      aura::Window* new_parent) {
  for (size_t i = 0; i < child->transient_children().size(); ++i) {
    ReparentChildWithTransientChildren(child->transient_children()[i],
                                       old_parent,
                                       new_parent);
  }
}

}  // namespace wm
}  // namespace ash
