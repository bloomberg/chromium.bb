// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_controller.h"

#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/laser/laser_pointer_view.h"
#include "ash/shell.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// A point gets removed from the collection if it is older than
// |kPointLifeDurationMs|.
const int kPointLifeDurationMs = 200;

// When no move events are being received we add a new point every
// |kAddStationaryPointsDelayMs| so that points older than
// |kPointLifeDurationMs| can get removed.
const int kAddStationaryPointsDelayMs = 5;

}  // namespace

LaserPointerController::LaserPointerController()
    : stationary_timer_(new base::Timer(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAddStationaryPointsDelayMs),
          base::Bind(&LaserPointerController::AddStationaryPoint,
                     base::Unretained(this)),
          true /* is_repeating */)) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

LaserPointerController::~LaserPointerController() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void LaserPointerController::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;

  enabled_ = enabled;
  if (!enabled_)
    laser_pointer_view_.reset();
}

void LaserPointerController::OnTouchEvent(ui::TouchEvent* event) {
  if (!enabled_)
    return;

  if (event->pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  if (event->type() != ui::ET_TOUCH_MOVED &&
      event->type() != ui::ET_TOUCH_PRESSED &&
      event->type() != ui::ET_TOUCH_RELEASED)
    return;

  // Find the root window that the event was captured on. We never need to
  // switch between different root windows because it is not physically possible
  // to seamlessly drag a finger between two displays like it is with a mouse.
  gfx::Point event_location = event->root_location();
  aura::Window* current_window =
      static_cast<aura::Window*>(event->target())->GetRootWindow();

  // Start a new laser session if the stylus is pressed but not pressed over the
  // palette.
  if (event->type() == ui::ET_TOUCH_PRESSED &&
      !palette_utils::PaletteContainsPointInScreen(event_location)) {
    DestroyLaserPointerView();
    UpdateLaserPointerView(current_window, event_location, event);
  }

  // Do not update laser if it is in the process of fading away.
  if (event->type() == ui::ET_TOUCH_MOVED && laser_pointer_view_ &&
      !is_fading_away_) {
    UpdateLaserPointerView(current_window, event_location, event);
    RestartTimer();
  }

  if (event->type() == ui::ET_TOUCH_RELEASED && laser_pointer_view_ &&
      !is_fading_away_) {
    is_fading_away_ = true;
    UpdateLaserPointerView(current_window, event_location, event);
    RestartTimer();
  }
}

void LaserPointerController::OnWindowDestroying(aura::Window* window) {
  SwitchTargetRootWindowIfNeeded(window);
}

void LaserPointerController::SwitchTargetRootWindowIfNeeded(
    aura::Window* root_window) {
  if (!root_window) {
    DestroyLaserPointerView();
  }

  if (!laser_pointer_view_ && enabled_) {
    laser_pointer_view_.reset(new LaserPointerView(
        base::TimeDelta::FromMilliseconds(kPointLifeDurationMs), root_window));
  }
}

void LaserPointerController::UpdateLaserPointerView(
    aura::Window* current_window,
    const gfx::Point& event_location,
    ui::Event* event) {
  SwitchTargetRootWindowIfNeeded(current_window);
  current_stylus_location_ = event_location;
  laser_pointer_view_->AddNewPoint(current_stylus_location_);
  event->StopPropagation();
}

void LaserPointerController::DestroyLaserPointerView() {
  // |stationary_timer_| should also be stopped so that it does not attempt to
  // add points when |laser_pointer_view_| is null.
  stationary_timer_->Stop();
  if (laser_pointer_view_) {
    is_fading_away_ = false;
    laser_pointer_view_.reset();
  }
}

void LaserPointerController::RestartTimer() {
  stationary_timer_repeat_count_ = 0;
  if (!stationary_timer_->IsRunning())
    stationary_timer_->Reset();
}

void LaserPointerController::AddStationaryPoint() {
  if (is_fading_away_)
    laser_pointer_view_->UpdateTime();
  else
    laser_pointer_view_->AddNewPoint(current_stylus_location_);

  // We can stop repeating the timer once the stylus has been stationary for
  // longer than the life of a point.
  if (stationary_timer_repeat_count_ * kAddStationaryPointsDelayMs >=
      kPointLifeDurationMs) {
    stationary_timer_->Stop();
    // Reset the view if the timer expires and the view was in process of fading
    // away.
    if (is_fading_away_)
      DestroyLaserPointerView();
  }
  stationary_timer_repeat_count_++;
}
}  // namespace ash
