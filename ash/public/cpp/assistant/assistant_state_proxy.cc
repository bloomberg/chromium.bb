// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "ash/public/cpp/assistant/assistant_state_proxy.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

AssistantStateProxy::AssistantStateProxy()
    : voice_interaction_observer_binding_(this) {}

AssistantStateProxy::~AssistantStateProxy() = default;

void AssistantStateProxy::Init(service_manager::Connector* connector) {
  connector->BindInterface(ash::mojom::kServiceName,
                           &voice_interaction_controller_);

  ash::mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_observer_binding_.Bind(mojo::MakeRequest(&ptr));
  voice_interaction_controller_->AddObserver(std::move(ptr));
}

void AssistantStateProxy::AddObserver(
    DefaultVoiceInteractionObserver* observer) {
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

  observers_.AddObserver(observer);
}

void AssistantStateProxy::RemoveObserver(
    DefaultVoiceInteractionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantStateProxy::OnVoiceInteractionStatusChanged(
    ash::mojom::VoiceInteractionState state) {
  voice_interaction_state_ = state;
  for (auto& observer : observers_)
    observer.OnVoiceInteractionStatusChanged(voice_interaction_state_.value());
}

void AssistantStateProxy::OnVoiceInteractionSettingsEnabled(bool enabled) {
  settings_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnVoiceInteractionSettingsEnabled(settings_enabled_.value());
}

void AssistantStateProxy::OnVoiceInteractionContextEnabled(bool enabled) {
  context_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnVoiceInteractionContextEnabled(context_enabled_.value());
}

void AssistantStateProxy::OnVoiceInteractionHotwordEnabled(bool enabled) {
  hotword_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnVoiceInteractionHotwordEnabled(hotword_enabled_.value());
}

void AssistantStateProxy::OnVoiceInteractionSetupCompleted(bool completed) {
  setup_completed_ = completed;
  for (auto& observer : observers_)
    observer.OnVoiceInteractionSetupCompleted(setup_completed_.value());
}

void AssistantStateProxy::OnAssistantFeatureAllowedChanged(
    ash::mojom::AssistantAllowedState state) {
  allowed_state_ = state;
  for (auto& observer : observers_)
    observer.OnAssistantFeatureAllowedChanged(allowed_state_.value());
}

void AssistantStateProxy::OnLocaleChanged(const std::string& locale) {
  locale_ = locale;
  for (auto& observer : observers_)
    observer.OnLocaleChanged(locale_.value());
}

}  // namespace ash
