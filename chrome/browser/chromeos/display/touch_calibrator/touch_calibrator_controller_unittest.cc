// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/touch_calibrator/touch_calibrator_controller.h"

#include <algorithm>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chrome/browser/chromeos/display/touch_calibrator/touch_calibrator_view.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"

using namespace display;

namespace chromeos {

class TouchCalibratorControllerTest : public ash::test::AshTestBase {
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
    EXPECT_FALSE(ctrl->is_calibrating());
    EXPECT_FALSE(!!ctrl->touch_calibrator_views_.size());

    TouchCalibratorController::TouchCalibrationCallback empty_callback;

    ctrl->StartCalibration(target_display, empty_callback);
    EXPECT_TRUE(ctrl->is_calibrating());

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

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchCalibratorControllerTest);
};

TEST_F(TouchCalibratorControllerTest, StartCalibration) {
  const Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::EventTargetTestApi test_api(ash::Shell::GetInstance());
  const ui::EventHandlerList& handlers = test_api.pre_target_handlers();
  ui::EventHandlerList::const_iterator event_target =
      std::find(handlers.begin(), handlers.end(), &touch_calibrator_controller);
  EXPECT_NE(event_target, handlers.end());
}

TEST_F(TouchCalibratorControllerTest, KeyEventIntercept) {
  const Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::test::EventGenerator& eg = GetEventGenerator();
  EXPECT_TRUE(touch_calibrator_controller.is_calibrating());
  eg.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(touch_calibrator_controller.is_calibrating());
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

}  // namespace chromeos
