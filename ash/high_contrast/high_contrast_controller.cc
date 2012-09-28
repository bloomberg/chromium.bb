// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/high_contrast/high_contrast_controller.h"

#include "ash/shell.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer.h"

namespace ash {

HighContrastController::HighContrastController()
    : enabled_(false) {
}

void HighContrastController::SetEnabled(bool enabled) {
  enabled_ = enabled;

  // Update all active displays.
  Shell::RootWindowList root_window_list = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator it = root_window_list.begin();
      it != root_window_list.end(); it++) {
    UpdateDisplay(*it);
  }
}

void HighContrastController::OnRootWindowAdded(aura::RootWindow* root_window) {
  UpdateDisplay(root_window);
}

void HighContrastController::UpdateDisplay(aura::RootWindow* root_window) {
  root_window->layer()->SetLayerInverted(enabled_);
}

}  // namespace ash
