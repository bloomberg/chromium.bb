// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_voice_interaction_framework_instance.h"

#include <utility>

namespace arc {

FakeVoiceInteractionFrameworkInstance::FakeVoiceInteractionFrameworkInstance() =
    default;

FakeVoiceInteractionFrameworkInstance::
    ~FakeVoiceInteractionFrameworkInstance() = default;

void FakeVoiceInteractionFrameworkInstance::Init(
    mojom::VoiceInteractionFrameworkHostPtr host_ptr) {}

void FakeVoiceInteractionFrameworkInstance::StartVoiceInteractionSession(
    bool homescreen_is_active) {
  start_session_count_++;
}

void FakeVoiceInteractionFrameworkInstance::ToggleVoiceInteractionSession(
    bool homescreen_is_active) {
  toggle_session_count_++;
}

void FakeVoiceInteractionFrameworkInstance::
    StartVoiceInteractionSessionForRegion(const gfx::Rect& region) {
  start_session_for_region_count_++;
  selected_region_ = region;
}

void FakeVoiceInteractionFrameworkInstance::SetMetalayerVisibility(
    bool visible) {
  set_metalayer_visibility_count_++;
  metalayer_visible_ = visible;
}

void FakeVoiceInteractionFrameworkInstance::SetVoiceInteractionEnabled(
    bool enable,
    SetVoiceInteractionEnabledCallback callback) {
  std::move(callback).Run();
}

void FakeVoiceInteractionFrameworkInstance::SetVoiceInteractionContextEnabled(
    bool enable) {}

void FakeVoiceInteractionFrameworkInstance::StartVoiceInteractionSetupWizard() {
  setup_wizard_count_++;
}

void FakeVoiceInteractionFrameworkInstance::ShowVoiceInteractionSettings() {
  show_settings_count_++;
}

void FakeVoiceInteractionFrameworkInstance::GetVoiceInteractionSettings(
    GetVoiceInteractionSettingsCallback callback) {}

}  // namespace arc
