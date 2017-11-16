// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accelerators/accelerator_controller_delegate_mash.h"

#include "ash/mus/window_manager.h"
#include "base/logging.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"

namespace ash {

AcceleratorControllerDelegateMash::AcceleratorControllerDelegateMash(
    WindowManager* window_manager)
    : window_manager_(window_manager) {}

AcceleratorControllerDelegateMash::~AcceleratorControllerDelegateMash() {}

bool AcceleratorControllerDelegateMash::HandlesAction(
    AcceleratorAction action) {
  // Accelerators that return true need to work differently in mash. These
  // should have implementations in CanPerformAction() and PerformAction().
  // Accelerators that return false have not been ported to work with mash yet.
  // If the behavior between cash and mash can be unified then the accelerator
  // should be moved to accelerator_controller.cc/h. See
  // http://crbug.com/612331.
  switch (action) {
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return true;
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
    case UNPIN:
      NOTIMPLEMENTED_LOG_ONCE();
      return false;
    default:
      break;
  }
  return false;
}

bool AcceleratorControllerDelegateMash::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator,
    const ui::Accelerator& previous_accelerator) {
  switch (action) {
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return true;
    default:
      break;
  }
  return false;
}

void AcceleratorControllerDelegateMash::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  switch (action) {
    case TOUCH_HUD_PROJECTION_TOGGLE: {
      mash::mojom::LaunchablePtr launchable;
      window_manager_->connector()->BindInterface("touch_hud", &launchable);
      launchable->Launch(mash::mojom::kWindow,
                         mash::mojom::LaunchMode::DEFAULT);
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace ash
