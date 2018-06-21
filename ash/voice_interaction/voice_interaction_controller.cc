// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/voice_interaction/voice_interaction_controller.h"

#include <utility>

namespace ash {

VoiceInteractionController::VoiceInteractionController() : binding_(this) {}

VoiceInteractionController::~VoiceInteractionController() = default;

void VoiceInteractionController::BindRequest(
    mojom::VoiceInteractionControllerRequest request) {
  binding_.Bind(std::move(request));
}

void VoiceInteractionController::AddVoiceInteractionObserver(
    mojom::VoiceInteractionObserverPtr observer) {
  observers_.AddPtr(std::move(observer));
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
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionContextEnabled(enabled);
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

void VoiceInteractionController::IsSettingEnabled(
    IsSettingEnabledCallback callback) {
  std::move(callback).Run(settings_enabled_);
}

void VoiceInteractionController::IsSetupCompleted(
    IsSetupCompletedCallback callback) {
  std::move(callback).Run(setup_completed_);
}

void VoiceInteractionController::FlushForTesting() {
  observers_.FlushForTesting();
}

}  // namespace ash
