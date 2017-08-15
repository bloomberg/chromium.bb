// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_VIEW_H_
#define ASH_LASER_LASER_POINTER_VIEW_H_

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/fast_ink/fast_ink_view.h"

namespace ash {

// LaserPointerView displays the palette tool laser pointer. It draws the laser,
// which consists of a point where the mouse cursor should be, as well as a
// trail of lines to help users track.
class LaserPointerView : public FastInkView {
 public:
  LaserPointerView(base::TimeDelta life_duration,
                   base::TimeDelta presentation_delay,
                   base::TimeDelta stationary_point_delay,
                   aura::Window* root_window);
  ~LaserPointerView() override;

  void AddNewPoint(const gfx::PointF& new_point,
                   const base::TimeTicks& new_time);

  void FadeOut(const base::Closure& done);

 private:
  friend class LaserPointerControllerTestApi;

  gfx::Rect GetBoundingBox();

  void AddPoint(const gfx::PointF& point, const base::TimeTicks& time);

  // Timer callback which adds a point where the stylus was last seen.
  // This allows the trail to fade away when the stylus is stationary.
  void UpdateTime();

  // FastInkView:
  void OnRedraw(gfx::Canvas& canvas) override;

  FastInkPoints laser_points_;
  FastInkPoints predicted_laser_points_;
  const base::TimeDelta presentation_delay_;

  // Timer which will add a new stationary point when the stylus stops moving.
  // This will remove points that are too old.
  std::unique_ptr<base::Timer> stationary_timer_;

  // A callback for when the fadeout is complete.
  base::Closure fadeout_done_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerView);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_VIEW_H_
