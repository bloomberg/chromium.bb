// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_POINTS_TESTAPI_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_POINTS_TESTAPI_H_

#include <memory>

#include "base/time/time.h"

namespace ash {

class LaserPointerPoints;

// An api for testing the laser_pointer_points class.
class LaserPointerPointsTestApi {
 public:
  LaserPointerPointsTestApi(std::unique_ptr<LaserPointerPoints> instance);
  ~LaserPointerPointsTestApi();

  int GetNumberOfPoints() const;
  // Moves existing points back in time by |delta| and adds a new point at
  // (0,0).
  void MoveForwardInTime(const base::TimeDelta& delta);

 private:
  // The time the new points are added.
  base::Time new_point_time_;
  std::unique_ptr<LaserPointerPoints> instance_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerPointsTestApi);
};
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_POINTS_TESTAPI_H_
