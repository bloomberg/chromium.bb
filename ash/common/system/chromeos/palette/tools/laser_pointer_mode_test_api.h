// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_MODE_TESTAPI_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_MODE_TESTAPI_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

class LaserPointerMode;
class LaserPointerPoints;

// An api for testing the laser_pointer_mode class.
class LaserPointerModeTestApi {
 public:
  LaserPointerModeTestApi(std::unique_ptr<LaserPointerMode> instance);
  ~LaserPointerModeTestApi();

  void OnEnable();
  void OnDisable();
  const LaserPointerPoints& laser_points();
  const gfx::Point& current_mouse_location();

 private:
  std::unique_ptr<LaserPointerMode> instance_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerModeTestApi);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_MODE_TESTAPI_H_
