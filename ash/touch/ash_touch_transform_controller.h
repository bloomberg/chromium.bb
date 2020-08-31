// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_ASH_TOUCH_TRANSFORM_CONTROLLER_H_
#define ASH_TOUCH_ASH_TOUCH_TRANSFORM_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "ui/display/manager/touch_transform_controller.h"

namespace display {
class DisplayManager;
}

namespace ash {

// AshTouchTransformController listens for display configuration changes and
// updates the touch transforms when one occurs.
class ASH_EXPORT AshTouchTransformController
    : public display::TouchTransformController,
      public WindowTreeHostManager::Observer {
 public:
  AshTouchTransformController(
      display::DisplayManager* display_manager,
      std::unique_ptr<display::TouchTransformSetter> setter);
  ~AshTouchTransformController() override;

  // WindowTreeHostManager::Observer:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AshTouchTransformController);
};

}  // namespace ash

#endif  // ASH_TOUCH_ASH_TOUCH_TRANSFORM_CONTROLLER_H_
