// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include "ash/wm/activation_controller.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

const char kOpenWindowSplitKey[] = "OpenWindowSplit";

}  // namespace

void ActivateWindow(aura::Window* window) {
  aura::client::GetActivationClient()->ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  aura::client::GetActivationClient()->DeactivateWindow(window);
}

bool IsActiveWindow(aura::Window* window) {
  return GetActiveWindow() == window;
}

aura::Window* GetActiveWindow() {
  return aura::client::GetActivationClient()->GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return internal::ActivationController::GetActivatableWindow(window);
}

namespace window_util {

bool IsWindowMaximized(aura::Window* window) {
  return window->GetIntProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

bool IsWindowFullscreen(aura::Window* window) {
  return window->GetIntProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_FULLSCREEN;
}

void MaximizeWindow(aura::Window* window) {
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void RestoreWindow(aura::Window* window) {
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

bool HasFullscreenWindow(const WindowSet& windows) {
  for (WindowSet::const_iterator i = windows.begin(); i != windows.end(); ++i) {
    if ((*i)->GetIntProperty(aura::client::kShowStateKey)
        == ui::SHOW_STATE_FULLSCREEN) {
      return true;
    }
  }
  return false;
}

void SetOpenWindowSplit(aura::Window* window, bool value) {
  window->SetIntProperty(kOpenWindowSplitKey, value ? 1 : 0);
}

bool GetOpenWindowSplit(aura::Window* window) {
  return window->GetIntProperty(kOpenWindowSplitKey) == 1;
}

}  // namespace window_util
}  // namespace ash
