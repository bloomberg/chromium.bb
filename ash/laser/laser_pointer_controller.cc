// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_controller.h"

#include "ash/ash_switches.h"
#include "ash/laser/laser_pointer_view.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_utils.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// A point gets removed from the collection if it is older than
// |kPointLifeDurationMs|.
const int kPointLifeDurationMs = 200;

// When no move events are being received we add a new point every
// |kAddStationaryPointsDelayMs| so that points older than
// |kPointLifeDurationMs| can get removed.
// Note: Using a delay less than the screen refresh interval will not
// provide a visual benefit but instead just waste time performing
// unnecessary updates. 16ms is the refresh interval on most devices.
// TODO(reveman): Use real VSYNC interval instead of a hard-coded delay.
const int kAddStationaryPointsDelayMs = 16;

// The default amount of time used to estimate time from VSYNC event to when
// visible light can be noticed by the user. This is used when a device
// specific estimate was not provided using --estimated-presentation-delay.
const int kDefaultPresentationDelayMs = 18;

}  // namespace

LaserPointerController::LaserPointerController()
    : stationary_timer_(new base::Timer(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAddStationaryPointsDelayMs),
          base::Bind(&LaserPointerController::AddStationaryPoint,
                     base::Unretained(this)),
          true /* is_repeating */)) {
  Shell::Get()->AddPreTargetHandler(this);

  int64_t presentation_delay_ms;
  // Use device specific presentation delay if specified.
  std::string presentation_delay_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshEstimatedPresentationDelay);
  if (!base::StringToInt64(presentation_delay_string, &presentation_delay_ms))
    presentation_delay_ms = kDefaultPresentationDelayMs;
  presentation_delay_ =
      base::TimeDelta::FromMilliseconds(presentation_delay_ms);
}

LaserPointerController::~LaserPointerController() {
  Shell::Get()->RemovePreTargetHandler(this);
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
    UpdateLaserPointerView(current_window, event->root_location_f(), event);
  }

  // Do not update laser if it is in the process of fading away.
  if (event->type() == ui::ET_TOUCH_MOVED && laser_pointer_view_ &&
      !is_fading_away_) {
    UpdateLaserPointerView(current_window, event->root_location_f(), event);
    RestartTimer();
  }

  if (event->type() == ui::ET_TOUCH_RELEASED && laser_pointer_view_ &&
      !is_fading_away_) {
    is_fading_away_ = true;
    UpdateLaserPointerView(current_window, event->root_location_f(), event);
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
        base::TimeDelta::FromMilliseconds(kPointLifeDurationMs),
        presentation_delay_, root_window));
  }
}

void LaserPointerController::UpdateLaserPointerView(
    aura::Window* current_window,
    const gfx::PointF& event_location,
    ui::Event* event) {
  SwitchTargetRootWindowIfNeeded(current_window);
  current_stylus_location_ = event_location;
  laser_pointer_view_->AddNewPoint(current_stylus_location_,
                                   event->time_stamp());
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
  stationary_timer_->Reset();
}

void LaserPointerController::AddStationaryPoint() {
  if (is_fading_away_) {
    laser_pointer_view_->UpdateTime();
  } else {
    laser_pointer_view_->AddNewPoint(current_stylus_location_,
                                     ui::EventTimeForNow());
  }

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
