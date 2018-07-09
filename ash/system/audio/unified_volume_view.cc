// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/unified/top_shortcut_button.h"

using chromeos::CrasAudioHandler;

namespace ash {

namespace {

// Threshold to ignore update on the slider value.
const float kSliderIgnoreUpdateThreshold = 0.01;

// References to the icons that correspond to different volume levels.
const gfx::VectorIcon* const kVolumeLevelIcons[] = {
    &kUnifiedMenuVolumeMuteIcon,    // Muted.
    &kUnifiedMenuVolumeLowIcon,     // Low volume.
    &kUnifiedMenuVolumeMediumIcon,  // Medium volume.
    &kUnifiedMenuVolumeHighIcon,    // High volume.
    &kUnifiedMenuVolumeHighIcon,    // Full volume.
};

// The maximum index of kVolumeLevelIcons.
constexpr int kVolumeLevels = arraysize(kVolumeLevelIcons) - 1;

// Get vector icon reference that corresponds to the given volume level. |level|
// is between 0.0 to 1.0.
const gfx::VectorIcon& GetVolumeIconForLevel(float level) {
  int index = static_cast<int>(std::ceil(level * kVolumeLevels));
  if (index < 0)
    index = 0;
  else if (index > kVolumeLevels)
    index = kVolumeLevels;
  return *kVolumeLevelIcons[index];
}

}  // namespace

UnifiedVolumeView::UnifiedVolumeView(UnifiedVolumeSliderController* controller,
                                     bool is_main_view)
    : UnifiedSliderView(controller,
                        kSystemMenuVolumeHighIcon,
                        IDS_ASH_STATUS_TRAY_VOLUME),
      more_button_(new TopShortcutButton(controller,
                                         kSystemMenuArrowRightIcon,
                                         IDS_ASH_STATUS_TRAY_AUDIO)),
      is_main_view_(is_main_view) {
  DCHECK(CrasAudioHandler::IsInitialized());
  CrasAudioHandler::Get()->AddAudioObserver(this);
  AddChildView(more_button_);
  Update(false /* by_user */);
}

UnifiedVolumeView::~UnifiedVolumeView() {
  DCHECK(CrasAudioHandler::IsInitialized());
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void UnifiedVolumeView::Update(bool by_user) {
  bool is_muted = CrasAudioHandler::Get()->IsOutputMuted();
  float level = CrasAudioHandler::Get()->GetOutputVolumePercent() / 100.f;

  // Indicate that the slider is inactive when it's muted.
  slider()->UpdateState(!is_muted);

  button()->SetToggled(!is_muted);
  button()->SetVectorIcon(GetVolumeIconForLevel(is_muted ? 0.f : level));

  more_button_->SetVisible(is_main_view_ &&
                           (CrasAudioHandler::Get()->has_alternative_input() ||
                            CrasAudioHandler::Get()->has_alternative_output()));

  // Slider's value is in finer granularity than audio volume level(0.01),
  // there will be a small discrepancy between slider's value and volume level
  // on audio side. To avoid the jittering in slider UI, do not set change
  // slider value if the change is less than the threshold.
  if (std::abs(level - slider()->value()) < kSliderIgnoreUpdateThreshold)
    return;

  SetSliderValue(level, by_user);
}

void UnifiedVolumeView::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                  int volume) {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnOutputMuteChanged(bool mute_on, bool system_adjust) {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnAudioNodesChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnActiveOutputNodeChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnActiveInputNodeChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::ChildVisibilityChanged(views::View* child) {
  Update(true /* by_user */);
}

}  // namespace ash
