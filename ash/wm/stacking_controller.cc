// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/stacking_controller.h"

#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/container_finder.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// StackingController, public:

StackingController::StackingController() {}

StackingController::~StackingController() {}

////////////////////////////////////////////////////////////////////////////////
// StackingController, aura::client::WindowTreeClient implementation:

aura::Window* StackingController::GetDefaultParent(aura::Window* context,
                                                   aura::Window* window,
                                                   const gfx::Rect& bounds) {
  return wm::WmWindowAura::GetAuraWindow(wm::GetDefaultParent(
      wm::WmWindowAura::Get(context), wm::WmWindowAura::Get(window), bounds));
}

}  // namespace ash
