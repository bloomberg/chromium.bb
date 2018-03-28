// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_H_

namespace chromeos {
namespace power {
namespace ml {

class ScreenBrightnessEvent;

// Interface to log a ScreenBrightnessEvent to UKM.
class AdaptiveScreenBrightnessUkmLogger {
 public:
  virtual ~AdaptiveScreenBrightnessUkmLogger() = default;

  // Log screen brightness proto.
  virtual void LogActivity(const ScreenBrightnessEvent& screen_brightness) = 0;
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_H_
