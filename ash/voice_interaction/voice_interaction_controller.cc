// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/voice_interaction/voice_interaction_controller.h"

#include <utility>

#include "chromeos/constants/chromeos_switches.h"

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
  if (voice_interaction_state_ == state)
    return;

  voice_interaction_state_ = state;
  observers_.ForAllPtrs([state](auto* observer) {
    observer->OnVoiceInteractionStatusChanged(state);
  });
  for (auto& observer : local_observers_)
    observer.OnVoiceInteractionStatusChanged(state);
}

void VoiceInteractionController::NotifySettingsEnabled(bool enabled) {
  if (settings_enabled_.has_value() && settings_enabled_.value() == enabled)
    return;

  settings_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionSettingsEnabled(enabled);
  });
  for (auto& observer : local_observers_)
    observer.OnVoiceInteractionSettingsEnabled(enabled);
}

void VoiceInteractionController::NotifyContextEnabled(bool enabled) {
  if (context_enabled_.has_value() && context_enabled_.value() == enabled)
    return;

  context_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionContextEnabled(enabled);
  });
  for (auto& observer : local_observers_)
    observer.OnVoiceInteractionContextEnabled(enabled);
}

void VoiceInteractionController::NotifyHotwordEnabled(bool enabled) {
  if (hotword_enabled_.has_value() && hotword_enabled_.value() == enabled)
    return;

  hotword_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionHotwordEnabled(enabled);
  });
  for (auto& observer : local_observers_)
    observer.OnVoiceInteractionHotwordEnabled(enabled);
}

void VoiceInteractionController::NotifyHotwordAlwaysOn(bool always_on) {
  if (hotword_always_on_.has_value() && hotword_always_on_.value() == always_on)
    return;

  hotword_always_on_ = always_on;
  observers_.ForAllPtrs([always_on](auto* observer) {
    observer->OnVoiceInteractionHotwordAlwaysOn(always_on);
  });
  for (auto& observer : local_observers_)
    observer.OnVoiceInteractionHotwordAlwaysOn(always_on);
}

void VoiceInteractionController::NotifySetupCompleted(bool completed) {
  if (setup_completed_.has_value() && setup_completed_.value() == completed)
    return;

  setup_completed_ = completed;
  observers_.ForAllPtrs([completed](auto* observer) {
    observer->OnVoiceInteractionSetupCompleted(completed);
  });
  for (auto& observer : local_observers_)
    observer.OnVoiceInteractionSetupCompleted(completed);
}

void VoiceInteractionController::NotifyFeatureAllowed(
    mojom::AssistantAllowedState state) {
  if (allowed_state_ == state)
    return;

  allowed_state_ = state;
  observers_.ForAllPtrs([state](auto* observer) {
    observer->OnAssistantFeatureAllowedChanged(state);
  });
  for (auto& observer : local_observers_)
    observer.OnAssistantFeatureAllowedChanged(state);
}

void VoiceInteractionController::NotifyNotificationEnabled(bool enabled) {
  notification_enabled_ = enabled;
}

void VoiceInteractionController::NotifyLocaleChanged(
    const std::string& locale) {
  if (locale_ == locale)
    return;

  locale_ = locale;
  observers_.ForAllPtrs(
      [locale](auto* observer) { observer->OnLocaleChanged(locale); });
  for (auto& observer : local_observers_)
    observer.OnLocaleChanged(locale);
}

void VoiceInteractionController::NotifyLaunchWithMicOpen(
    bool launch_with_mic_open) {
  launch_with_mic_open_ = launch_with_mic_open;
}

void VoiceInteractionController::AddObserver(
    mojom::VoiceInteractionObserverPtr observer) {
  InitObserver(observer.get());
  observers_.AddPtr(std::move(observer));
}

void VoiceInteractionController::AddLocalObserver(
    DefaultVoiceInteractionObserver* observer) {
  InitObserver(observer);
  local_observers_.AddObserver(observer);
}

void VoiceInteractionController::RemoveLocalObserver(
    DefaultVoiceInteractionObserver* observer) {
  local_observers_.RemoveObserver(observer);
}

void VoiceInteractionController::InitObserver(
    mojom::VoiceInteractionObserver* observer) {
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
  if (hotword_always_on_.has_value())
    observer->OnVoiceInteractionHotwordAlwaysOn(hotword_always_on_.value());
  if (allowed_state_.has_value())
    observer->OnAssistantFeatureAllowedChanged(allowed_state_.value());
  if (locale_.has_value())
    observer->OnLocaleChanged(locale_.value());
}

void VoiceInteractionController::FlushForTesting() {
  observers_.FlushForTesting();
}

}  // namespace ash
