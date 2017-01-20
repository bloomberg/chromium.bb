// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/touch_calibrator/touch_calibrator_controller.h"

#include "ash/shell.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/display/touch_calibrator/touch_calibrator_view.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {

// Time interval after a touch event during which all other touch events are
// ignored during calibration.
const base::TimeDelta TouchCalibratorController::kTouchIntervalThreshold =
    base::TimeDelta::FromMilliseconds(200);

TouchCalibratorController::TouchCalibratorController()
    : last_touch_timestamp_(base::Time::Now()) {}

TouchCalibratorController::~TouchCalibratorController() {
  touch_calibrator_views_.clear();
  StopCalibration();
}

void TouchCalibratorController::OnDisplayConfigurationChanged() {
  touch_calibrator_views_.clear();
  StopCalibration();
}

void TouchCalibratorController::StartCalibration(
    const display::Display& target_display,
    const TouchCalibratorController::TouchCalibrationCallback& callback) {
  is_calibrating_ = true;
  callback_ = callback;

  ash::Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
  target_display_ = target_display;

  // Clear all touch calibrator views used in any previous calibration.
  touch_calibrator_views_.clear();

  // Reset the calibration data.
  touch_point_quad_.fill(std::make_pair(gfx::Point(0, 0), gfx::Point(0, 0)));

  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();

  for (const display::Display& display : displays) {
    bool is_primary_view = display.id() == target_display_.id();
    touch_calibrator_views_[display.id()] =
        base::MakeUnique<TouchCalibratorView>(display, is_primary_view);
  }

  ash::Shell::GetInstance()->touch_transformer_controller()->SetForCalibration(
      true);

  // Add self as an event handler target.
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

void TouchCalibratorController::StopCalibration() {
  if (!is_calibrating_)
    return;
  is_calibrating_ = false;

  ash::Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);

  ash::Shell::GetInstance()->touch_transformer_controller()->SetForCalibration(
      false);

  // Remove self as the event handler.
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);

  // Transition all touch calibrator views to their final state for a graceful
  // exit.
  for (const auto& it : touch_calibrator_views_)
    it.second->SkipToFinalState();

  if (callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback_, false));
    callback_.Reset();
  }
}

// ui::EventHandler:
void TouchCalibratorController::OnKeyEvent(ui::KeyEvent* key) {
  if (!is_calibrating_)
    return;
  // Detect ESC key press.
  if (key->type() == ui::ET_KEY_PRESSED && key->key_code() == ui::VKEY_ESCAPE)
    StopCalibration();

  key->StopPropagation();
}

void TouchCalibratorController::OnTouchEvent(ui::TouchEvent* touch) {
  if (!is_calibrating_)
    return;
  if (touch->type() != ui::ET_TOUCH_RELEASED)
    return;
  if (base::Time::Now() - last_touch_timestamp_ < kTouchIntervalThreshold)
    return;
  last_touch_timestamp_ = base::Time::Now();

  TouchCalibratorView* target_screen_calibration_view =
      touch_calibrator_views_[target_display_.id()].get();

  // If this is the final state, then store all calibration data and stop
  // calibration.
  if (target_screen_calibration_view->state() ==
      TouchCalibratorView::CALIBRATION_COMPLETE) {
    if (callback_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(callback_, true));
      callback_.Reset();
    }
    StopCalibration();
    ash::Shell::GetInstance()->display_manager()->SetTouchCalibrationData(
        target_display_.id(), touch_point_quad_,
        target_screen_calibration_view->size());
    return;
  }

  int state_index;
  // Maps the state to an integer value. Assigns a non negative integral value
  // for a state in which the user can interact with the the interface.
  switch (target_screen_calibration_view->state()) {
    case TouchCalibratorView::DISPLAY_POINT_1:
      state_index = 0;
      break;
    case TouchCalibratorView::DISPLAY_POINT_2:
      state_index = 1;
      break;
    case TouchCalibratorView::DISPLAY_POINT_3:
      state_index = 2;
      break;
    case TouchCalibratorView::DISPLAY_POINT_4:
      state_index = 3;
      break;
    default:
      // Return early if the interface is in a state that does not allow user
      // interaction.
      return;
  }

  // Store touch point corresponding to its display point.
  gfx::Point display_point;
  if (target_screen_calibration_view->GetDisplayPointLocation(&display_point)) {
    touch_point_quad_[state_index] =
        std::make_pair(display_point, touch->location());
  } else {
    // TODO(malaykeshav): Display some kind of error for the user.
    NOTREACHED() << "Touch calibration failed. Could not retrieve location for"
                    " display point. Retry calibration.";
  }

  target_screen_calibration_view->AdvanceToNextState();
}

}  // namespace chromeos
