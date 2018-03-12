// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/brightness_control_delegate.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace ash {

namespace {

// The default value of the slider that is shown until GetBrightnessPercent() is
// complete.
const float kDefaultSliderValue = 1.f;

}  // namespace

UnifiedBrightnessView::UnifiedBrightnessView(
    UnifiedBrightnessSliderController* controller)
    : UnifiedSliderView(controller,
                        kSystemMenuBrightnessIcon,
                        IDS_ASH_STATUS_TRAY_BRIGHTNESS) {
  slider()->SetValue(kDefaultSliderValue);

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  Shell::Get()->brightness_control_delegate()->GetBrightnessPercent(
      base::BindOnce(&UnifiedBrightnessView::HandleInitialBrightness,
                     weak_ptr_factory_.GetWeakPtr()));
}

UnifiedBrightnessView::~UnifiedBrightnessView() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void UnifiedBrightnessView::HandleInitialBrightness(
    base::Optional<double> percent) {
  if (percent.has_value())
    slider()->SetValue(percent.value() / 100.);
}

void UnifiedBrightnessView::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_BRIGHTNESS_CHANGED);
  slider()->SetValue(change.percent() / 100.);
}

}  // namespace ash
