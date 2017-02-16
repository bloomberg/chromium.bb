// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/material_design/material_design_controller.h"

#include <string>

namespace ash {

// static
bool MaterialDesignController::IsShelfMaterial() {
  return true;
}

// static
bool MaterialDesignController::IsSystemTrayMenuMaterial() {
  return true;
}

// static
bool MaterialDesignController::UseMaterialDesignSystemIcons() {
  return true;
}

}  // namespace ash
