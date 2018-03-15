// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_slider_controller.h"

#include "ash/system/audio/unified_volume_view.h"

using chromeos::CrasAudioHandler;

namespace ash {

UnifiedVolumeSliderController::UnifiedVolumeSliderController() = default;
UnifiedVolumeSliderController::~UnifiedVolumeSliderController() = default;

views::View* UnifiedVolumeSliderController::CreateView() {
  DCHECK(!slider_);
  slider_ = new UnifiedVolumeView(this);
  return slider_;
}

void UnifiedVolumeSliderController::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  CrasAudioHandler::Get()->SetOutputMute(
      !CrasAudioHandler::Get()->IsOutputMuted());
}

void UnifiedVolumeSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::VALUE_CHANGED_BY_USER)
    return;

  const int level = value * 100;

  CrasAudioHandler::Get()->SetOutputVolumePercent(level);

  // If the volume is above certain level and it's muted, it should be unmuted.
  // If the volume is below certain level and it's unmuted, it should be muted.
  if (CrasAudioHandler::Get()->IsOutputMuted() ==
      level > CrasAudioHandler::Get()->GetOutputDefaultVolumeMuteThreshold()) {
    CrasAudioHandler::Get()->SetOutputMute(
        !CrasAudioHandler::Get()->IsOutputMuted());
  }
}

}  // namespace ash
