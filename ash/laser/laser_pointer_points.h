// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_POINTS_H_
#define ASH_LASER_LASER_POINTER_POINTS_H_

#include <deque>
#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// LaserPointerPoints is a helper class used for displaying the palette tool
// laser pointer. It keeps track of the points needed to render the laser
// pointer and its tail.
class ASH_EXPORT LaserPointerPoints {
 public:
  // Struct to describe each point.
  struct LaserPoint {
    gfx::Point location;
    // age is a value between [0,1] where 0 means the point was just added and 1
    // means that the point is just about to be removed.
    float age = 0.0f;
  };

  // Constructor with a parameter to choose the fade out time of the points in
  // the collection.
  explicit LaserPointerPoints(base::TimeDelta life_duration);
  ~LaserPointerPoints();

  // Adds a point. Automatically clears points that are too old.
  void AddPoint(const gfx::Point& point);
  // Updates the collection latest time. Automatically clears points that are
  // too old.
  void MoveForwardToTime(const base::Time& latest_time);
  // Removes all points.
  void Clear();
  // Gets the bounding box of the points.
  gfx::Rect GetBoundingBox();
  // Returns the oldest point in the collection.
  LaserPoint GetOldest() const;
  // Returns the newest point in the collection.
  LaserPoint GetNewest() const;
  // Returns the number of points in the collection.
  int GetNumberOfPoints() const;
  // Whether there are any points or not.
  bool IsEmpty() const;
  // Expose the collection so callers can work with the points.
  const std::deque<LaserPoint>& laser_points();

 private:
  friend class LaserPointerPointsTestApi;

  base::TimeDelta life_duration_;
  std::deque<LaserPoint> points_;
  // The latest time of the collection of points. This gets updated when new
  // points are added or when MoveForwardInTime is called.
  base::Time collection_latest_time_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerPoints);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_POINTS_H_
