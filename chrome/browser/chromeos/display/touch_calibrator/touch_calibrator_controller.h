// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_TOUCH_CALIBRATOR_TOUCH_CALIBRATOR_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_TOUCH_CALIBRATOR_TOUCH_CALIBRATOR_CONTROLLER_H_

#include <map>

#include "ash/display/window_tree_host_manager.h"
#include "base/time/time.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/events/event_handler.h"

namespace ui {
class KeyEvent;
class TouchEvent;
}

namespace chromeos {

class TouchCalibratorView;

// TouchCalibratorController is responsible for collecting the touch calibration
// associated data from the user. It instantiates TouchCalibratorView classes to
// present an interface the user can interact with for calibration.
// Touch calibration is restricted to calibrate only one display at a time.
class TouchCalibratorController : public ui::EventHandler,
                                  public ash::WindowTreeHostManager::Observer {
 public:
  using CalibrationPointPairQuad =
      display::TouchCalibrationData::CalibrationPointPairQuad;
  using TouchCalibrationCallback = base::Callback<void(bool)>;

  static const base::TimeDelta kTouchIntervalThreshold;

  TouchCalibratorController();
  ~TouchCalibratorController() override;

  // ui::EventHandler
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // WindowTreeHostManager::Observer
  void OnDisplayConfigurationChanged() override;

  // Starts the calibration process for the given |target_display|.
  void StartCalibration(const display::Display& target_display,
                        const TouchCalibrationCallback& callback);

  // Stops any ongoing calibration process.
  void StopCalibration();

  bool is_calibrating() { return is_calibrating_; }

 private:
  friend class TouchCalibratorControllerTest;
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest, StartCalibration);
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest, KeyEventIntercept);
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest, TouchThreshold);

  // A map for TouchCalibrator view with the key as display id of the display
  // it is present in.
  std::map<int64_t, std::unique_ptr<TouchCalibratorView>>
      touch_calibrator_views_;

  // The display which is being calibrated by the touch calibrator controller.
  // This is valid only if |is_calibrating| is set to true.
  display::Display target_display_;

  // During calibration this stores the timestamp when the previous touch event
  // was received.
  base::Time last_touch_timestamp_;

  // Is true if a touch calibration is already underprocess.
  bool is_calibrating_ = false;

  // An array of Calibration point pairs. This stores all the 4 display and
  // touch input point pairs that will be used for calibration.
  CalibrationPointPairQuad touch_point_quad_;

  // A callback to be called when touch calibration completes.
  TouchCalibrationCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TouchCalibratorController);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_TOUCH_CALIBRATOR_TOUCH_CALIBRATOR_CONTROLLER_H_
