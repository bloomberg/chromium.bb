// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/touch_calibrator_controller.h"

#include <memory>

#include "ash/display/touch_calibrator_view.h"
#include "ash/shell.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {
namespace {

void InitInternalTouchDeviceIds(std::set<int>& internal_touch_device_ids) {
  if (!display::Display::HasInternalDisplay())
    return;

  DCHECK(ui::DeviceDataManager::GetInstance()
             ->AreTouchscreenTargetDisplaysValid());

  internal_touch_device_ids.clear();

  const std::vector<ui::TouchscreenDevice>& device_list =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  int64_t internal_display_id = display::Display::InternalDisplayId();

  for (const auto& touchscreen_device : device_list) {
    if (touchscreen_device.target_display_id == internal_display_id)
      internal_touch_device_ids.insert(touchscreen_device.id);
  }
}

}  // namespace

// Time interval after a touch event during which all other touch events are
// ignored during calibration.
const base::TimeDelta TouchCalibratorController::kTouchIntervalThreshold =
    base::TimeDelta::FromMilliseconds(200);

TouchCalibratorController::TouchCalibratorController()
    : last_touch_timestamp_(base::Time::Now()) {}

TouchCalibratorController::~TouchCalibratorController() {
  touch_calibrator_views_.clear();
  StopCalibrationAndResetParams();
}

void TouchCalibratorController::OnDisplayConfigurationChanged() {
  touch_calibrator_views_.clear();
  StopCalibrationAndResetParams();
}

void TouchCalibratorController::StartCalibration(
    const display::Display& target_display,
    bool is_custom_calibration,
    TouchCalibrationCallback opt_callback) {
  state_ = is_custom_calibration ? CalibrationState::kCustomCalibration
                                 : CalibrationState::kNativeCalibration;

  if (opt_callback)
    opt_callback_ = std::move(opt_callback);

  target_display_ = target_display;

  // Clear all touch calibrator views used in any previous calibration.
  touch_calibrator_views_.clear();

  // Set the touch device id as invalid so it can be set during calibration.
  touch_device_id_ = ui::InputDevice::kInvalidId;

  // Populate |internal_touch_device_ids_| with the ids of touch devices that
  // are currently associated with the internal display.
  InitInternalTouchDeviceIds(internal_touch_device_ids_);

  // If this is a native touch calibration, then initialize the UX for it.
  if (state_ == CalibrationState::kNativeCalibration) {
    Shell::Get()->window_tree_host_manager()->AddObserver(this);

    // Reset the calibration data.
    touch_point_quad_.fill(std::make_pair(gfx::Point(0, 0), gfx::Point(0, 0)));

    std::vector<display::Display> displays =
        display::Screen::GetScreen()->GetAllDisplays();

    for (const display::Display& display : displays) {
      bool is_primary_view = display.id() == target_display_.id();
      touch_calibrator_views_[display.id()] =
          std::make_unique<TouchCalibratorView>(display, is_primary_view);
    }
  }

  Shell::Get()->touch_transformer_controller()->SetForCalibration(true);

  // Add self as an event handler target.
  Shell::Get()->AddPreTargetHandler(this);
}

void TouchCalibratorController::StopCalibrationAndResetParams() {
  if (!IsCalibrating())
    return;
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);

  Shell::Get()->touch_transformer_controller()->SetForCalibration(false);

  // Remove self as the event handler.
  Shell::Get()->RemovePreTargetHandler(this);

  // Transition all touch calibrator views to their final state for a graceful
  // exit if this is touch calibration with native UX.
  if (state_ == CalibrationState::kNativeCalibration) {
    for (const auto& it : touch_calibrator_views_)
      it.second->SkipToFinalState();
  }

  state_ = CalibrationState::kInactive;

  if (opt_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(opt_callback_), false /* failure */));
    opt_callback_.Reset();
  }
}

void TouchCalibratorController::CompleteCalibration(
    const CalibrationPointPairQuad& pairs,
    const gfx::Size& display_size) {
  bool did_find_touch_device = false;
  uint32_t touch_device_identifier =
      display::TouchCalibrationData::GetFallbackTouchDeviceIdentifier();

  const std::vector<ui::TouchscreenDevice>& device_list =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  for (const auto& device : device_list) {
    if (device.id == touch_device_id_) {
      touch_device_identifier =
          display::TouchCalibrationData::GenerateTouchDeviceIdentifier(device);
      did_find_touch_device = true;
      break;
    }
  }

  if (!did_find_touch_device) {
    VLOG(1) << "No touch device with id: " << touch_device_id_ << " found to "
            << "complete touch calibration for display with id: "
            << target_display_.id() << ". Storing it as a fallback";
  } else if (touch_device_identifier ==
             display::TouchCalibrationData::
                 GetFallbackTouchDeviceIdentifier()) {
    LOG(ERROR)
        << "Hash collision in generating touch device identifier for "
        << " device. Hash Generated: " << touch_device_identifier
        << " || Fallback touch device identifier: "
        << display::TouchCalibrationData::GetFallbackTouchDeviceIdentifier();
  }

  if (opt_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(opt_callback_), true /* success */));
    opt_callback_.Reset();
  }
  StopCalibrationAndResetParams();
  Shell::Get()->display_manager()->SetTouchCalibrationData(
      target_display_.id(), pairs, display_size, touch_device_identifier);
}

bool TouchCalibratorController::IsCalibrating() const {
  return state_ != CalibrationState::kInactive;
}

// ui::EventHandler:
void TouchCalibratorController::OnKeyEvent(ui::KeyEvent* key) {
  if (state_ != CalibrationState::kNativeCalibration)
    return;
  // Detect ESC key press.
  if (key->type() == ui::ET_KEY_PRESSED && key->key_code() == ui::VKEY_ESCAPE)
    StopCalibrationAndResetParams();

  key->StopPropagation();
}

void TouchCalibratorController::OnTouchEvent(ui::TouchEvent* touch) {
  if (!IsCalibrating())
    return;
  if (touch->type() != ui::ET_TOUCH_RELEASED)
    return;
  if (base::Time::Now() - last_touch_timestamp_ < kTouchIntervalThreshold)
    return;
  last_touch_timestamp_ = base::Time::Now();

  // If the touch event originated from a touch device that is associated with
  // the internal display, then ignore it.
  if (internal_touch_device_ids_.count(touch->source_device_id()))
    return;

  if (touch_device_id_ == ui::InputDevice::kInvalidId)
    touch_device_id_ = touch->source_device_id();

  // If this is a custom touch calibration, then everything else is managed
  // by the application responsible for the custom calibration UX.
  if (state_ == CalibrationState::kCustomCalibration)
    return;

  TouchCalibratorView* target_screen_calibration_view =
      touch_calibrator_views_[target_display_.id()].get();

  // If this is the final state, then store all calibration data and stop
  // calibration.
  if (target_screen_calibration_view->state() ==
      TouchCalibratorView::CALIBRATION_COMPLETE) {
    CompleteCalibration(touch_point_quad_,
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

}  // namespace ash
