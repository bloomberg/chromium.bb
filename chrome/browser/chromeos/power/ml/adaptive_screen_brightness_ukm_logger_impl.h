// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_ukm_logger.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace chromeos {
namespace power {
namespace ml {

class AdaptiveScreenBrightnessUkmLoggerImpl
    : public AdaptiveScreenBrightnessUkmLogger {
 public:
  AdaptiveScreenBrightnessUkmLoggerImpl() = default;
  ~AdaptiveScreenBrightnessUkmLoggerImpl() override;

  // chromeos::power::ml::AdaptiveScreenBrightnessUkmLogger overrides:
  void LogActivity(const ScreenBrightnessEvent& screen_brightness_event,
                   ukm::SourceId tab_id,
                   bool has_form_entry) override;

 private:
  // This ID is incremented each time a ScreenBrightessEvent is logged to UKM.
  // Event index resets when a new user session starts.
  int next_sequence_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(AdaptiveScreenBrightnessUkmLoggerImpl);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_IMPL_H_
