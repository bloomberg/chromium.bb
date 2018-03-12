// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_slider_controller.h"

#include "ash/shell.h"
#include "ash/system/brightness/tray_brightness.h"
#include "ash/system/brightness/unified_brightness_view.h"
#include "ash/system/brightness_control_delegate.h"

namespace ash {

UnifiedBrightnessSliderController::UnifiedBrightnessSliderController() =
    default;
UnifiedBrightnessSliderController::~UnifiedBrightnessSliderController() =
    default;

views::View* UnifiedBrightnessSliderController::CreateView() {
  DCHECK(!slider_);
  slider_ = new UnifiedBrightnessView(this);
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
  if (brightness_control_delegate) {
    double percent =
        std::max<float>(value * 100.f, tray::kMinBrightnessPercent);
    brightness_control_delegate->SetBrightnessPercent(percent, true);
  }
}

}  // namespace ash
