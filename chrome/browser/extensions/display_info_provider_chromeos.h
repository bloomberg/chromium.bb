// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "extensions/browser/api/system_display/display_info_provider.h"

namespace ash {
class OverscanCalibrator;
class TouchCalibratorController;
}  // namespace ash

namespace extensions {

class DisplayInfoProviderChromeOS : public DisplayInfoProvider {
 public:
  DisplayInfoProviderChromeOS();
  ~DisplayInfoProviderChromeOS() override;

  // DisplayInfoProvider implementation.
  void SetDisplayProperties(
      const std::string& display_id,
      const api::system_display::DisplayProperties& properties,
      ErrorCallback callback) override;
  void SetDisplayLayout(const DisplayLayoutList& layouts,
                        ErrorCallback callback) override;
  void EnableUnifiedDesktop(bool enable) override;
  void GetAllDisplaysInfo(
      bool single_unified,
      base::OnceCallback<void(DisplayUnitInfoList result)> callback) override;
  void GetDisplayLayout(
      base::OnceCallback<void(DisplayLayoutList result)> callback) override;
  bool OverscanCalibrationStart(const std::string& id) override;
  bool OverscanCalibrationAdjust(
      const std::string& id,
      const api::system_display::Insets& delta) override;
  bool OverscanCalibrationReset(const std::string& id) override;
  bool OverscanCalibrationComplete(const std::string& id) override;
  void ShowNativeTouchCalibration(const std::string& id,
                                  ErrorCallback callback) override;
  bool IsNativeTouchCalibrationActive() override;
  bool StartCustomTouchCalibration(const std::string& id) override;
  bool CompleteCustomTouchCalibration(
      const api::system_display::TouchCalibrationPairQuad& pairs,
      const api::system_display::Bounds& bounds) override;
  bool ClearTouchCalibration(const std::string& id) override;
  bool IsCustomTouchCalibrationActive() override;
  void SetMirrorMode(const api::system_display::MirrorModeInfo& info,
                     ErrorCallback callback) override;
  void UpdateDisplayUnitInfoForPlatform(
      const display::Display& display,
      api::system_display::DisplayUnitInfo* unit) override;

 private:
  ash::TouchCalibratorController* GetTouchCalibrator();

  ash::OverscanCalibrator* GetOverscanCalibrator(const std::string& id);

  std::map<std::string, std::unique_ptr<ash::OverscanCalibrator>>
      overscan_calibrators_;

  std::unique_ptr<ash::TouchCalibratorController> touch_calibrator_;

  std::string touch_calibration_target_id_;
  bool custom_touch_calibration_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderChromeOS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_CHROMEOS_H_
