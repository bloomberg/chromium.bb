// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <vector>

#include "ash/shell.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/display.h"
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

bool CanActivateWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->CanActivateWindow(window);
}

bool IsWindowNormal(aura::Window* window) {
  return IsWindowStateNormal(window->GetProperty(aura::client::kShowStateKey));
}

bool IsWindowStateNormal(ui::WindowShowState state) {
  return state == ui::SHOW_STATE_NORMAL || state == ui::SHOW_STATE_DEFAULT;
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
  const gfx::Display display = gfx::Screen::GetDisplayNearestWindow(window);
  gfx::Rect center = display.work_area().Center(window->bounds().size());
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

}  // namespace wm
}  // namespace ash
