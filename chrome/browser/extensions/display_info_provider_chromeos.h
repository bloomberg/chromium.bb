// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "extensions/browser/api/system_display/display_info_provider.h"

namespace chromeos {
class OverscanCalibrator;
class TouchCalibratorController;
}

namespace extensions {

class DisplayInfoProviderChromeOS : public DisplayInfoProvider {
 public:
  DisplayInfoProviderChromeOS();
  ~DisplayInfoProviderChromeOS() override;

  // DisplayInfoProvider implementation.
  bool SetInfo(const std::string& display_id,
               const api::system_display::DisplayProperties& info,
               std::string* error) override;
  bool SetDisplayLayout(const DisplayLayoutList& layouts) override;
  void UpdateDisplayUnitInfoForPlatform(
      const display::Display& display,
      api::system_display::DisplayUnitInfo* unit) override;
  void EnableUnifiedDesktop(bool enable) override;
  DisplayUnitInfoList GetAllDisplaysInfo() override;
  DisplayLayoutList GetDisplayLayout() override;
  bool OverscanCalibrationStart(const std::string& id) override;
  bool OverscanCalibrationAdjust(
      const std::string& id,
      const api::system_display::Insets& delta) override;
  bool OverscanCalibrationReset(const std::string& id) override;
  bool OverscanCalibrationComplete(const std::string& id) override;
  bool TouchCalibrationSet(
      const std::string& id,
      const api::system_display::TouchCalibrationPairQuad& pairs,
      const api::system_display::Bounds& bounds,
      std::string* error) override;
  bool TouchCalibrationReset(const std::string& id,
                             std::string* error) override;
  bool IsTouchCalibrationActive(std::string* error) override;

 private:
  chromeos::TouchCalibratorController* GetTouchCalibrator();

  chromeos::OverscanCalibrator* GetOverscanCalibrator(const std::string& id);

  std::map<std::string, std::unique_ptr<chromeos::OverscanCalibrator>>
      overscan_calibrators_;

  std::unique_ptr<chromeos::TouchCalibratorController> touch_calibrator_;

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderChromeOS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_CHROMEOS_H_
