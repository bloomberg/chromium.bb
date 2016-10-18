// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_POINTS_TEST_API_H_
#define ASH_LASER_LASER_POINTER_POINTS_TEST_API_H_

#include "ash/laser/laser_pointer_points.h"
#include "base/time/time.h"

namespace ash {

// An api for testing the laser_pointer_points class.
class LaserPointerPointsTestApi {
 public:
  LaserPointerPointsTestApi(LaserPointerPoints* instance);
  ~LaserPointerPointsTestApi();

  int GetNumberOfPoints() const;
  // Moves existing points back in time by |delta| and adds a new point at
  // (0,0).
  void MoveForwardInTime(const base::TimeDelta& delta);
  LaserPointerPoints::LaserPoint GetPointAtIndex(int index);

 private:
  LaserPointerPoints* instance_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerPointsTestApi);
};
}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_POINTS_TEST_API_H_
