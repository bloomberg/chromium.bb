// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/work_area_insets.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/window.h"

namespace ash {

// static
WorkAreaInsets* WorkAreaInsets::ForWindow(const aura::Window* window) {
  return RootWindowController::ForWindow(window)->work_area_insets();
}

WorkAreaInsets::WorkAreaInsets(RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller) {}

WorkAreaInsets::~WorkAreaInsets() = default;

void WorkAreaInsets::SetDockedMagnifierHeight(int height) {
  docked_magnifier_height_ = height;
  Shell::Get()->NotifyAccessibilityInsetsChanged(
      root_window_controller_->GetRootWindow());
}

void WorkAreaInsets::SetAccessibilityPanelHeight(int height) {
  accessibility_panel_height_ = height;
  Shell::Get()->NotifyAccessibilityInsetsChanged(
      root_window_controller_->GetRootWindow());
}

}  // namespace ash