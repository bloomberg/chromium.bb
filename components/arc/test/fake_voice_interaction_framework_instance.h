// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_VOICE_INTERACTION_FRAMEWORK_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_VOICE_INTERACTION_FRAMEWORK_INSTANCE_H_

#include <stddef.h>

#include "components/arc/common/voice_interaction_framework.mojom.h"

namespace arc {

class FakeVoiceInteractionFrameworkInstance
    : public mojom::VoiceInteractionFrameworkInstance {
 public:
  FakeVoiceInteractionFrameworkInstance();
  ~FakeVoiceInteractionFrameworkInstance() override;

  // mojom::VoiceInteractionFrameworkInstance overrides:
  void Init(mojom::VoiceInteractionFrameworkHostPtr host_ptr) override;
  void StartVoiceInteractionSession(bool homescreen_is_active) override;
  void ToggleVoiceInteractionSession(bool homescreen_is_active) override;
  void StartVoiceInteractionSessionForRegion(const gfx::Rect& region) override;
  void SetMetalayerVisibility(bool visible) override;
  void SetVoiceInteractionEnabled(bool enable) override;
  void SetVoiceInteractionContextEnabled(bool enable) override;
  void StartVoiceInteractionSetupWizard() override;
  void ShowVoiceInteractionSettings() override;
  void GetVoiceInteractionSettings(
      GetVoiceInteractionSettingsCallback callback) override;

  size_t start_session_count() const { return start_session_count_; }
  size_t toggle_session_count() const { return toggle_session_count_; }
  size_t setup_wizard_count() const { return setup_wizard_count_; }
  size_t show_settings_count() const { return show_settings_count_; }

 private:
  size_t start_session_count_ = 0u;
  size_t toggle_session_count_ = 0u;
  size_t setup_wizard_count_ = 0u;
  size_t show_settings_count_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeVoiceInteractionFrameworkInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_VOICE_INTERACTION_FRAMEWORK_INSTANCE_H_
