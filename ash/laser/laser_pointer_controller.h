// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_CONTROLLER_H_
#define ASH_LASER_LASER_POINTER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

namespace base {
class Timer;
}

namespace ash {

class LaserPointerView;

// Controller for the laser pointer functionality. Enables/disables laser
// pointer as well as receives points and passes them off to be rendered.
class ASH_EXPORT LaserPointerController : public ui::EventHandler,
                                          public aura::WindowObserver {
 public:
  LaserPointerController();
  ~LaserPointerController() override;

  // Turns the laser pointer feature on or off. The user still has to press and
  // drag to see the laser pointer.
  void SetEnabled(bool enabled);

 private:
  friend class LaserPointerControllerTestApi;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Handles monitor disconnect/swap primary.
  void SwitchTargetRootWindowIfNeeded(aura::Window* root_window);

  // Timer callback which adds a point where the stylus was last seen. This
  // allows the trail to fade away when the stylus is stationary.
  void AddStationaryPoint();

  // Updates |laser_pointer_view_| by changing its root window or creating it if
  // needed. Also adds the latest point at |event_location| and stops
  // propagation of |event|.
  void UpdateLaserPointerView(aura::Window* current_window,
                              const gfx::Point& event_location,
                              ui::Event* event);

  // Destroys |laser_pointer_view_|, if it exists.
  void DestroyLaserPointerView();

  void RestartTimer();

  // Timer which will add a new stationary point when the stylus stops moving.
  // This will remove points that are too old.
  std::unique_ptr<base::Timer> stationary_timer_;
  int stationary_timer_repeat_count_ = 0;

  bool enabled_ = false;

  // |is_fading_away_| determines whether the laser pointer view should accept
  // points normally, or just advance the |laser_points_| time so that current
  // points start fading away. This should be set to true when the view is about
  // to be destroyed, such as when the stylus is released.
  bool is_fading_away_ = false;

  // The last seen stylus location in screen coordinates.
  gfx::Point current_stylus_location_;

  // |laser_pointer_view_| will only hold an instance when the laser pointer is
  // enabled and activated (pressed or dragged).
  std::unique_ptr<LaserPointerView> laser_pointer_view_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerController);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_CONTROLLER_H_
