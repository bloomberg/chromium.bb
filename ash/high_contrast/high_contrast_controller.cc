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
  root_window_ = ash::Shell::GetPrimaryRootWindow();
}

void HighContrastController::SetEnabled(bool enabled) {
  enabled_ = enabled;

  root_window_->layer()->SetLayerInverted(enabled_);
}

}  // namespace ash
