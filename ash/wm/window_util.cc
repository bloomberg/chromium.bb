// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include "ash/shell.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/monitor.h"
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
  return aura::client::GetActivationClient(Shell::GetRootWindow())->
      GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return internal::ActivationController::GetActivatableWindow(window, NULL);
}

bool CanActivateWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->CanActivateWindow(window);
}

bool IsWindowNormal(aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey) ==
          ui::SHOW_STATE_NORMAL ||
      window->GetProperty(aura::client::kShowStateKey) ==
          ui::SHOW_STATE_DEFAULT;
}

bool IsWindowMaximized(aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

bool IsWindowMinimized(aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED;
}

bool IsWindowFullscreen(aura::Window* window) {
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

void CenterWindow(aura::Window* window) {
  const gfx::Monitor monitor = gfx::Screen::GetMonitorNearestWindow(window);
  gfx::Rect center = monitor.work_area().Center(window->bounds().size());
  window->SetBounds(center);
}

}  // namespace wm
}  // namespace ash
