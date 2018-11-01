// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/voice_interaction/voice_interaction_controller.h"

#include <utility>

#include "chromeos/chromeos_switches.h"

namespace ash {

VoiceInteractionController::VoiceInteractionController() {
  if (chromeos::switches::IsAssistantEnabled())
    voice_interaction_state_ = mojom::VoiceInteractionState::NOT_READY;
}

VoiceInteractionController::~VoiceInteractionController() = default;

void VoiceInteractionController::BindRequest(
    mojom::VoiceInteractionControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void VoiceInteractionController::NotifyStatusChanged(
    mojom::VoiceInteractionState state) {
  voice_interaction_state_ = state;
  observers_.ForAllPtrs([state](auto* observer) {
    observer->OnVoiceInteractionStatusChanged(state);
  });
}

void VoiceInteractionController::NotifySettingsEnabled(bool enabled) {
  settings_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionSettingsEnabled(enabled);
  });
}

void VoiceInteractionController::NotifyContextEnabled(bool enabled) {
  context_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionContextEnabled(enabled);
  });
}

void VoiceInteractionController::NotifyHotwordEnabled(bool enabled) {
  hotword_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionHotwordEnabled(enabled);
  });
}

void VoiceInteractionController::NotifySetupCompleted(bool completed) {
  setup_completed_ = completed;
  observers_.ForAllPtrs([completed](auto* observer) {
    observer->OnVoiceInteractionSetupCompleted(completed);
  });
}

void VoiceInteractionController::NotifyFeatureAllowed(
    mojom::AssistantAllowedState state) {
  allowed_state_ = state;
  observers_.ForAllPtrs([state](auto* observer) {
    observer->OnAssistantFeatureAllowedChanged(state);
  });
}

void VoiceInteractionController::NotifyNotificationEnabled(bool enabled) {
  notification_enabled_ = enabled;
}

void VoiceInteractionController::NotifyLocaleChanged(
    const std::string& locale) {
  locale_ = locale;
  observers_.ForAllPtrs(
      [locale](auto* observer) { observer->OnLocaleChanged(locale); });
}

void VoiceInteractionController::NotifyLaunchWithMicOpen(
    bool launch_with_mic_open) {
  launch_with_mic_open_ = launch_with_mic_open;
}


void VoiceInteractionController::AddObserver(
    mojom::VoiceInteractionObserverPtr observer) {
  if (voice_interaction_state_.has_value())
    observer->OnVoiceInteractionStatusChanged(voice_interaction_state_.value());
  if (settings_enabled_.has_value())
    observer->OnVoiceInteractionSettingsEnabled(settings_enabled_.value());
  if (context_enabled_.has_value())
    observer->OnVoiceInteractionContextEnabled(context_enabled_.value());
  if (hotword_enabled_.has_value())
    observer->OnVoiceInteractionHotwordEnabled(hotword_enabled_.value());
  if (setup_completed_.has_value())
    observer->OnVoiceInteractionSetupCompleted(setup_completed_.value());
  if (allowed_state_.has_value())
    observer->OnAssistantFeatureAllowedChanged(allowed_state_.value());
  if (locale_.has_value())
    observer->OnLocaleChanged(locale_.value());

  observers_.AddPtr(std::move(observer));
}

void VoiceInteractionController::FlushForTesting() {
  observers_.FlushForTesting();
}

}  // namespace ash
