// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_slider_controller.h"

#include "ash/shell.h"
#include "ash/system/brightness/tray_brightness.h"
#include "ash/system/brightness/unified_brightness_view.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/unified/unified_system_tray_model.h"

namespace ash {

UnifiedBrightnessSliderController::UnifiedBrightnessSliderController(
    UnifiedSystemTrayModel* model)
    : model_(model) {}

UnifiedBrightnessSliderController::~UnifiedBrightnessSliderController() =
    default;

views::View* UnifiedBrightnessSliderController::CreateView() {
  DCHECK(!slider_);
  slider_ = new UnifiedBrightnessView(this, model_);
  return slider_;
}

void UnifiedBrightnessSliderController::ButtonPressed(views::Button* sender,
                                                      const ui::Event& event) {
  // The button in is UnifiedBrightnessView is no-op.
}

void UnifiedBrightnessSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::VALUE_CHANGED_BY_USER)
    return;

  BrightnessControlDelegate* brightness_control_delegate =
      Shell::Get()->brightness_control_delegate();
  if (!brightness_control_delegate)
    return;

  double percent = value * 100.;
  // If previous percentage and current percentage are both below the minimum,
  // we don't update the actual brightness.
  if (percent < tray::kMinBrightnessPercent &&
      previous_percent_ < tray::kMinBrightnessPercent) {
    return;
  }
  // We have to store previous manually set value because |old_value| might be
  // set by UnifiedSystemTrayModel::Observer.
  previous_percent_ = percent;

  percent = std::max(tray::kMinBrightnessPercent, percent);
  brightness_control_delegate->SetBrightnessPercent(percent, true);
}

}  // namespace ash
