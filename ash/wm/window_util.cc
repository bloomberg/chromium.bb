// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/shell.h"
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

namespace ash {
namespace wm {

void ActivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  aura::client::GetActivationClient(window->GetRootWindow())->ActivateWindow(
      window);
}

void DeactivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  aura::client::GetActivationClient(window->GetRootWindow())->DeactivateWindow(
      window);
}

bool IsActiveWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->GetActiveWindow() == window;
}

aura::Window* GetActiveWindow() {
  return aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return internal::ActivationController::GetActivatableWindow(window, NULL);
}

bool IsActiveWindowFullscreen() {
  aura::Window* window = GetActiveWindow();
  while (window) {
    if (window->GetProperty(aura::client::kShowStateKey) ==
        ui::SHOW_STATE_FULLSCREEN) {
      return true;
    }
    window = window->parent();
  }
  return false;
}

bool CanActivateWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->CanActivateWindow(window);
}

bool CanMaximizeWindow(const aura::Window* window) {
  return window->GetProperty(aura::client::kCanMaximizeKey);
}

bool CanResizeWindow(const aura::Window* window) {
  return window->GetProperty(aura::client::kCanResizeKey);
}

bool CanSnapWindow(aura::Window* window) {
  // If a window has a maximum size defined, snapping may make it too big.
  return window->delegate()->GetMaximumSize().IsEmpty();
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
  window->SetBounds(center);
}

ui::Layer* RecreateWindowLayers(aura::Window* window, bool set_bounds) {
  const gfx::Rect bounds = window->bounds();
  ui::Layer* old_layer = window->RecreateLayer();
  DCHECK(old_layer);
  for (aura::Window::Windows::const_iterator it = window->children().begin();
       it != window->children().end();
       ++it) {
    // Maintain the hierarchy of the detached layers.
    old_layer->Add(RecreateWindowLayers(*it, set_bounds));
  }
  if (set_bounds)
    window->SetBounds(bounds);
  return old_layer;
}

void DeepDeleteLayers(ui::Layer* layer) {
  std::vector<ui::Layer*> children = layer->children();
  for (std::vector<ui::Layer*>::const_iterator it = children.begin();
       it != children.end();
       ++it) {
    ui::Layer* child = *it;
    DeepDeleteLayers(child);
  }
  delete layer;
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

void AdjustBoundsToEnsureWindowVisibility(gfx::Rect* bounds,
                                          const gfx::Rect& work_area) {
  bounds->set_width(std::min(bounds->width(), work_area.width()));
  bounds->set_height(std::min(bounds->height(), work_area.height()));
  if (!work_area.Intersects(*bounds)) {
    int y_offset = 0;
    if (work_area.bottom() < bounds->y()) {
      y_offset = work_area.bottom() - bounds->y() - kMinimumOnScreenArea;
    } else if (bounds->bottom() < work_area.y()) {
      y_offset = work_area.y() - bounds->bottom() + kMinimumOnScreenArea;
    }

    int x_offset = 0;
    if (work_area.right() < bounds->x()) {
      x_offset = work_area.right() - bounds->x() - kMinimumOnScreenArea;
    } else if (bounds->right() < work_area.x()) {
      x_offset = work_area.x() - bounds->right() + kMinimumOnScreenArea;
    }
    bounds->Offset(x_offset, y_offset);
  }
}

}  // namespace wm
}  // namespace ash
