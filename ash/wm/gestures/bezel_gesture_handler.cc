// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/bezel_gesture_handler.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/volume_control_delegate.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/screen.h"

namespace {

// Device bezel operation constants (volume/brightness slider).

// This is the minimal brightness value allowed for the display.
const double kMinBrightnessPercent = 5.0;
// For device operation, the finger is not allowed to enter the screen more
// then this fraction of the size of the screen.
const double kAllowableScreenOverlapForDeviceCommand = 0.0005;

// TODO(skuhne): The noise reduction can be removed when / if we are adding a
// more general reduction.
// To avoid unwanted noise activation, the first 'n' events are being ignored
// for bezel device gestures.
const int kIgnoreFirstBezelDeviceEvents = 10;
// Within these 'n' huge coordinate changes are not allowed. The threshold is
// given in fraction of screen resolution changes.
const double kBezelNoiseDeltaFilter = 0.1;
// To avoid the most frequent noise (extreme locations) the bezel percent
// sliders will not cover the entire screen. We scale therefore the percent
// value by this many percent for minima and maxima extension.
// (Range extends to -kMinMaxInsetPercent .. 100 + kMinMaxInsetPercent).
const double kMinMaxInsetPercent = 5.0;
// To make it possible to reach minimas and maximas easily a range extension
// of -kMinMaxCutOffPercent .. 100 + kMinMaxCutOffPercent will be clamped to
// 0..100%. Everything beyond that will be ignored.
const double kMinMaxCutOffPercent = 2.0;

}  // namespace

namespace ash {
namespace internal {

BezelGestureHandler::BezelGestureHandler()
    : overlap_percent_(5),
      start_location_(BEZEL_START_UNSET),
      orientation_(SCROLL_ORIENTATION_UNSET),
      is_scrubbing_(false),
      initiation_delay_events_(0) {
  enable_bezel_device_control_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
                ::switches::kEnableBezelTouch);
}

BezelGestureHandler::~BezelGestureHandler() {
}

void BezelGestureHandler::ProcessGestureEvent(aura::Window* target,
                                              const ui::GestureEvent& event) {
  switch (event.type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      HandleBezelGestureStart(target, event);

      // TODO(sad|skuhne): Fix the bezel gestures for when the shelf is on the
      //                   left/right of the screen.
      if (start_location_ == BEZEL_START_BOTTOM)
        shelf_handler_.ProcessGestureEvent(event);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      // Check if a valid start position has been set.
      if (start_location_ == BEZEL_START_UNSET)
        break;

      if (DetermineGestureOrientation(event))
        HandleBezelGestureUpdate(target, event);
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      if (start_location_ == BEZEL_START_BOTTOM)
        shelf_handler_.ProcessGestureEvent(event);
      HandleBezelGestureEnd();
      break;
    default:
      break;
  }
}

bool BezelGestureHandler::HandleDeviceControl(
    const gfx::Rect& screen,
    const ui::GestureEvent& event) {
  // Get the slider position as value from the absolute position.
  // Note that the highest value is at the top.
  double percent = 100.0 - 100.0 * (event.y() - screen.y()) / screen.height();
  if (!DeNoiseBezelSliderPosition(&percent)) {
    // Note: Even though this particular event might be noise, the gesture
    // itself is still valid and should not get cancelled.
    return false;
  }
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT) {
    ash::BrightnessControlDelegate* delegate =
        accelerator->brightness_control_delegate();
    if (delegate)
      delegate->SetBrightnessPercent(
          LimitBezelBrightnessFromSlider(percent), true);
  } else if (start_location_ == BEZEL_START_RIGHT) {
    Shell::GetInstance()->tray_delegate()->GetVolumeControlDelegate()->
        SetVolumePercent(percent);
  } else {
    // No further events are necessary.
    return true;
  }

  // More notifications can be send.
  return false;
}

bool BezelGestureHandler::HandleLauncherControl(const ui::GestureEvent& event) {
  CHECK_EQ(BEZEL_START_BOTTOM, start_location_);
  shelf_handler_.ProcessGestureEvent(event);
  return event.type() == ui::ET_GESTURE_SCROLL_END ||
         event.type() == ui::ET_SCROLL_FLING_START;
}

bool BezelGestureHandler::HandleApplicationControl(
    const ui::GestureEvent& event) {
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT && event.details().scroll_x() > 0)
    accelerator->PerformAction(CYCLE_BACKWARD_LINEAR, ui::Accelerator());
  else if (start_location_ == BEZEL_START_RIGHT &&
             event.details().scroll_x() < 0)
    accelerator->PerformAction(CYCLE_FORWARD_LINEAR, ui::Accelerator());
  else
    return false;

  // No further notifications for this gesture.
  return true;
}

void BezelGestureHandler::HandleBezelGestureStart(
    aura::Window* target,
    const ui::GestureEvent& event) {
  gfx::Rect screen = gfx::Screen::GetDisplayNearestWindow(target).bounds();
  int overlap_area = screen.width() * overlap_percent_ / 100;
  orientation_ = SCROLL_ORIENTATION_UNSET;

  if (event.x() <= screen.x() + overlap_area) {
    start_location_ = BEZEL_START_LEFT;
  } else if (event.x() >= screen.right() - overlap_area) {
    start_location_ = BEZEL_START_RIGHT;
  } else if (event.y() >= screen.bottom()) {
    start_location_ = BEZEL_START_BOTTOM;
  }
}

bool BezelGestureHandler::DetermineGestureOrientation(
    const ui::GestureEvent& event) {
  if (orientation_ == SCROLL_ORIENTATION_UNSET) {
    if (!event.details().scroll_x() && !event.details().scroll_y())
      return false;

    // For left and right the scroll angle needs to be much steeper to
    // be accepted for a 'device configuration' gesture.
    if (start_location_ == BEZEL_START_LEFT ||
        start_location_ == BEZEL_START_RIGHT) {
      orientation_ = abs(event.details().scroll_y()) >
                     abs(event.details().scroll_x()) * 3 ?
          SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
    } else {
      orientation_ = abs(event.details().scroll_y()) >
                     abs(event.details().scroll_x()) ?
          SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
    }

    // Reset the delay counter for noise event filtering.
    initiation_delay_events_ = 0;
  }
  return true;
}

void BezelGestureHandler::HandleBezelGestureUpdate(
    aura::Window* target,
    const ui::GestureEvent& event) {
  if (orientation_ == SCROLL_ORIENTATION_HORIZONTAL) {
    if (HandleApplicationControl(event))
      start_location_ = BEZEL_START_UNSET;
  } else {
    if (start_location_ == BEZEL_START_BOTTOM) {
      if (HandleLauncherControl(event))
        start_location_ = BEZEL_START_UNSET;
    } else {
      // Check if device gestures should be performed or not.
      if (!enable_bezel_device_control_) {
        start_location_ = BEZEL_START_UNSET;
        return;
      }
      gfx::Rect screen = gfx::Screen::GetDisplayNearestWindow(target).bounds();
      // Limit the user gesture "mostly" to the off screen area and check for
      // noise invocation.
      if (!GestureInBezelArea(screen, event) ||
          BezelGestureMightBeNoise(screen, event))
        return;
      if (HandleDeviceControl(screen, event))
        start_location_ = BEZEL_START_UNSET;
    }
  }
}

void BezelGestureHandler::HandleBezelGestureEnd() {
  // All which is needed is to set the gesture start location to undefined.
  start_location_ = BEZEL_START_UNSET;
}

bool BezelGestureHandler::GestureInBezelArea(
    const gfx::Rect& screen,
    const ui::GestureEvent& event) {
  // Limit the gesture mostly to the off screen.
  double allowable_offset =
      screen.width() * kAllowableScreenOverlapForDeviceCommand;
  if ((start_location_ == BEZEL_START_LEFT &&
       event.x() > allowable_offset) ||
      (start_location_ == BEZEL_START_RIGHT &&
       event.x() < screen.width() - allowable_offset)) {
    start_location_ = BEZEL_START_UNSET;
    return false;
  }
  return true;
}

bool BezelGestureHandler::BezelGestureMightBeNoise(
    const gfx::Rect& screen,
    const ui::GestureEvent& event) {
  // The first events will not trigger an action.
  if (initiation_delay_events_++ < kIgnoreFirstBezelDeviceEvents) {
    // When the values are too far apart we ignore it since it might
    // be random noise.
    double delta_y = event.details().scroll_y();
    double span_y = screen.height();
    if (abs(delta_y / span_y) > kBezelNoiseDeltaFilter)
      start_location_ = BEZEL_START_UNSET;
    return true;
  }
  return false;
}

bool BezelGestureHandler::DeNoiseBezelSliderPosition(double* percent) {
  // The range gets passed as 0..100% and is extended to the range of
  // (-kMinMaxInsetPercent) .. (100 + kMinMaxInsetPercent). This way we can
  // cut off the extreme upper and lower values which are prone to noise.
  // It additionally adds a "security buffer" which can then be clamped to the
  // extremes to empower the user to get to these values (0% and 100%).
  *percent = *percent * (100.0 + 2 * kMinMaxInsetPercent) / 100 -
      kMinMaxInsetPercent;
  // Values which fall outside of the acceptable inner range area get ignored.
  if (*percent < -kMinMaxCutOffPercent ||
      *percent > 100.0 + kMinMaxCutOffPercent)
    return false;
  // Excessive values get then clamped to the 0..100% range.
  *percent = std::max(std::min(*percent, 100.0), 0.0);
  return true;
}

double BezelGestureHandler::LimitBezelBrightnessFromSlider(double percent) {
  // Turning off the display makes no sense, so we map the accessible range to
  // kMinimumBrightness .. 100%.
  percent = (percent + kMinBrightnessPercent) * 100.0 /
      (100.0 + kMinBrightnessPercent);
  // Clamp to avoid rounding issues.
  return std::min(percent, 100.0);
}

}  // namespace internal
}  // namespace ash
