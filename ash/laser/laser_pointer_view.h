// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_VIEW_H_
#define ASH_LASER_LASER_POINTER_VIEW_H_

#include "ash/fast_ink/fast_ink_view.h"
#include "ash/laser/laser_pointer_points.h"
#include "base/time/time.h"

namespace gfx {
class PointF;
}

namespace ash {

// LaserPointerView displays the palette tool laser pointer. It draws the laser,
// which consists of a point where the mouse cursor should be, as well as a
// trail of lines to help users track.
class LaserPointerView : public FastInkView {
 public:
  LaserPointerView(base::TimeDelta life_duration,
                   base::TimeDelta presentation_delay,
                   aura::Window* root_window);
  ~LaserPointerView() override;

  void AddNewPoint(const gfx::PointF& new_point,
                   const base::TimeTicks& new_time);
  void UpdateTime();
  void Stop();

 private:
  friend class LaserPointerControllerTestApi;

  gfx::Rect GetBoundingBox();

  void OnRedraw(gfx::Canvas& canvas, const gfx::Vector2d& offset) override;

  LaserPointerPoints laser_points_;
  LaserPointerPoints predicted_laser_points_;
  const base::TimeDelta presentation_delay_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerView);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_VIEW_H_
