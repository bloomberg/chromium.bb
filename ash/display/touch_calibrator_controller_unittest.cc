// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/touch_calibrator_controller.h"

#include <vector>

#include "ash/display/touch_calibrator_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/stl_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/device_data_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"

using namespace display;

namespace ash {

class TouchCalibratorControllerTest : public AshTestBase {
 public:
  TouchCalibratorControllerTest() {}

  const Display& InitDisplays() {
    // Initialize 2 displays each with resolution 500x500.
    UpdateDisplay("500x500,500x500");
    // Assuming index 0 points to the native display, we will calibrate the
    // touch display at index 1.
    const int kTargetDisplayIndex = 1;
    DisplayIdList display_id_list =
        display_manager()->GetCurrentDisplayIdList();
    int64_t target_display_id = display_id_list[kTargetDisplayIndex];
    const Display& touch_display =
        display_manager()->GetDisplayForId(target_display_id);
    return touch_display;
  }

  void StartCalibrationChecks(TouchCalibratorController* ctrl,
                              const Display& target_display) {
    EXPECT_FALSE(ctrl->IsCalibrating());
    EXPECT_FALSE(!!ctrl->touch_calibrator_views_.size());

    TouchCalibratorController::TouchCalibrationCallback empty_callback;

    ctrl->StartCalibration(target_display, false /* is_custom_calibration */,
                           std::move(empty_callback));
    EXPECT_TRUE(ctrl->IsCalibrating());
    EXPECT_EQ(ctrl->state_,
              TouchCalibratorController::CalibrationState::kNativeCalibration);

    // There should be a touch calibrator view associated with each of the
    // active displays.
    EXPECT_EQ(ctrl->touch_calibrator_views_.size(),
              display_manager()->GetCurrentDisplayIdList().size());

    TouchCalibratorView* target_calibrator_view =
        ctrl->touch_calibrator_views_[target_display.id()].get();

    // End the background fade in animation.
    target_calibrator_view->SkipCurrentAnimation();

    // TouchCalibratorView on the display being calibrated should be at the
    // state where the first display point is visible.
    EXPECT_EQ(target_calibrator_view->state(),
              TouchCalibratorView::DISPLAY_POINT_1);
  }

  // Generates a touch press and release event in the |display| with source
  // device id as |touch_device_id|.
  void GenerateTouchEvent(const Display& display, int touch_device_id) {
    gfx::Vector2d offset(display_manager()
                             ->GetDisplayInfo(display.id())
                             .bounds_in_native()
                             .OffsetFromOrigin());
    ui::test::EventGenerator& eg = GetEventGenerator();
    ui::TouchEvent press_touch_event(
        ui::ET_TOUCH_PRESSED, gfx::Point(20, 20) + offset,
        ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 12, 1.0f,
                           1.0f, 0.0f),
        0, 0.0f);
    ui::TouchEvent release_touch_event(
        ui::ET_TOUCH_RELEASED, gfx::Point(20, 20) + offset,
        ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 12, 1.0f,
                           1.0f, 0.0f),
        0, 0.0f);

    press_touch_event.set_source_device_id(touch_device_id);
    release_touch_event.set_source_device_id(touch_device_id);

    eg.Dispatch(&press_touch_event);
    eg.Dispatch(&release_touch_event);
  }

  ui::TouchscreenDevice InitTouchDevice(int64_t display_id) {
    ui::DeviceDataManager::CreateInstance();
    ui::TouchscreenDevice touchdevice(
        12, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
        std::string("test touch device"), gfx::Size(1000, 1000), 1);

    ui::test::DeviceDataManagerTestAPI devices_test_api;
    devices_test_api.SetTouchscreenDevices({touchdevice});

    std::vector<ui::TouchDeviceTransform> transforms;
    ui::TouchDeviceTransform touch_device_transform;
    touch_device_transform.display_id = display_id;
    touch_device_transform.device_id = touchdevice.id;
    transforms.push_back(touch_device_transform);

    ui::DeviceDataManager::GetInstance()->ConfigureTouchDevices(transforms);
    return touchdevice;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchCalibratorControllerTest);
};

TEST_F(TouchCalibratorControllerTest, StartCalibration) {
  const Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::EventTargetTestApi test_api(Shell::Get());
  const ui::EventHandlerList& handlers = test_api.pre_target_handlers();
  EXPECT_TRUE(base::ContainsValue(handlers, &touch_calibrator_controller));
}

TEST_F(TouchCalibratorControllerTest, KeyEventIntercept) {
  const Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::test::EventGenerator& eg = GetEventGenerator();
  EXPECT_TRUE(touch_calibrator_controller.IsCalibrating());
  eg.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(touch_calibrator_controller.IsCalibrating());
}

TEST_F(TouchCalibratorControllerTest, TouchThreshold) {
  const Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::test::EventGenerator& eg = GetEventGenerator();

  base::Time current_timestamp = base::Time::Now();
  touch_calibrator_controller.last_touch_timestamp_ = current_timestamp;

  eg.set_current_location(gfx::Point(0, 0));
  eg.PressTouch();
  eg.ReleaseTouch();

  EXPECT_EQ(touch_calibrator_controller.last_touch_timestamp_,
            current_timestamp);

  current_timestamp = base::Time::Now();
  touch_calibrator_controller.last_touch_timestamp_ =
      current_timestamp - (TouchCalibratorController::kTouchIntervalThreshold);

  eg.PressTouch();
  eg.ReleaseTouch();

  EXPECT_LT(current_timestamp,
            touch_calibrator_controller.last_touch_timestamp_);
}

TEST_F(TouchCalibratorControllerTest, TouchDeviceIdIsSet) {
  const Display& touch_display = InitDisplays();
  ui::TouchscreenDevice touchdevice = InitTouchDevice(touch_display.id());

  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  base::Time current_timestamp = base::Time::Now();
  touch_calibrator_controller.last_touch_timestamp_ =
      current_timestamp - (TouchCalibratorController::kTouchIntervalThreshold);

  EXPECT_EQ(touch_calibrator_controller.touch_device_id_,
            ui::InputDevice::kInvalidId);
  GenerateTouchEvent(touch_display, touchdevice.id);
  EXPECT_EQ(touch_calibrator_controller.touch_device_id_, touchdevice.id);
}

TEST_F(TouchCalibratorControllerTest, CustomCalibration) {
  const Display& touch_display = InitDisplays();
  ui::TouchscreenDevice touchdevice = InitTouchDevice(touch_display.id());

  TouchCalibratorController touch_calibrator_controller;
  EXPECT_FALSE(touch_calibrator_controller.IsCalibrating());
  EXPECT_FALSE(!!touch_calibrator_controller.touch_calibrator_views_.size());

  touch_calibrator_controller.StartCalibration(
      touch_display, true /* is_custom_calibbration */,
      TouchCalibratorController::TouchCalibrationCallback());

  EXPECT_TRUE(touch_calibrator_controller.IsCalibrating());
  EXPECT_EQ(touch_calibrator_controller.state_,
            TouchCalibratorController::CalibrationState::kCustomCalibration);

  // Native touch calibration UX should not initialize during custom calibration
  EXPECT_EQ(touch_calibrator_controller.touch_calibrator_views_.size(), 0UL);
  EXPECT_EQ(touch_calibrator_controller.touch_device_id_,
            ui::InputDevice::kInvalidId);

  base::Time current_timestamp = base::Time::Now();
  touch_calibrator_controller.last_touch_timestamp_ =
      current_timestamp - (TouchCalibratorController::kTouchIntervalThreshold);

  GenerateTouchEvent(touch_display, touchdevice.id);
  EXPECT_EQ(touch_calibrator_controller.touch_device_id_, touchdevice.id);

  display::TouchCalibrationData::CalibrationPointPairQuad points = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size size(200, 100);
  display::TouchCalibrationData calibration_data(points, size);

  touch_calibrator_controller.CompleteCalibration(points, size);

  const display::ManagedDisplayInfo& info =
      display_manager()->GetDisplayInfo(touch_display.id());

  uint32_t touch_device_identifier =
      display::TouchCalibrationData::GenerateTouchDeviceIdentifier(
          touchdevice.name, touchdevice.vendor_id, touchdevice.product_id);
  EXPECT_TRUE(info.HasTouchCalibrationData(touch_device_identifier));
  EXPECT_EQ(calibration_data,
            info.GetTouchCalibrationData(touch_device_identifier));
}

TEST_F(TouchCalibratorControllerTest, CustomCalibrationInvalidTouchId) {
  const Display& touch_display = InitDisplays();

  TouchCalibratorController touch_calibrator_controller;
  EXPECT_FALSE(touch_calibrator_controller.IsCalibrating());
  EXPECT_FALSE(!!touch_calibrator_controller.touch_calibrator_views_.size());

  touch_calibrator_controller.StartCalibration(
      touch_display, true /* is_custom_calibbration */,
      TouchCalibratorController::TouchCalibrationCallback());

  EXPECT_TRUE(touch_calibrator_controller.IsCalibrating());
  EXPECT_EQ(touch_calibrator_controller.state_,
            TouchCalibratorController::CalibrationState::kCustomCalibration);

  // Native touch calibration UX should not initialize during custom calibration
  EXPECT_EQ(touch_calibrator_controller.touch_calibrator_views_.size(), 0UL);
  EXPECT_EQ(touch_calibrator_controller.touch_device_id_,
            ui::InputDevice::kInvalidId);

  base::Time current_timestamp = base::Time::Now();
  touch_calibrator_controller.last_touch_timestamp_ =
      current_timestamp - (TouchCalibratorController::kTouchIntervalThreshold);

  display::TouchCalibrationData::CalibrationPointPairQuad points = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size size(200, 100);
  display::TouchCalibrationData calibration_data(points, size);

  touch_calibrator_controller.CompleteCalibration(points, size);

  const display::ManagedDisplayInfo& info =
      display_manager()->GetDisplayInfo(touch_display.id());

  uint32_t random_touch_device_identifier = 123456;
  EXPECT_TRUE(info.HasTouchCalibrationData(random_touch_device_identifier));
  EXPECT_EQ(calibration_data,
            info.GetTouchCalibrationData(random_touch_device_identifier));
}

}  // namespace ash
