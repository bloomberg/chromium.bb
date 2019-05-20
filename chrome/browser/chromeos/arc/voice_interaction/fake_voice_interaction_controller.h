// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_FAKE_VOICE_INTERACTION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_FAKE_VOICE_INTERACTION_CONTROLLER_H_

#include <string>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class FakeVoiceInteractionController
    : public ash::mojom::VoiceInteractionController {
 public:
  FakeVoiceInteractionController();
  ~FakeVoiceInteractionController() override;

  ash::mojom::VoiceInteractionControllerPtr CreateInterfacePtrAndBind();

  // ash::mojom::VoiceInteractionController:
  void NotifyStatusChanged(ash::mojom::VoiceInteractionState state) override;
  void NotifySettingsEnabled(bool enabled) override;
  void NotifyContextEnabled(bool enabled) override;
  void NotifyHotwordAlwaysOn(bool enabled) override;
  void NotifyHotwordEnabled(bool enabled) override;
  void NotifyConsentStatus(ash::mojom::ConsentStatus consent_status) override;
  void NotifyFeatureAllowed(ash::mojom::AssistantAllowedState state) override;
  void NotifyNotificationEnabled(bool enabled) override;
  void NotifyLocaleChanged(const std::string& locale) override;
  void NotifyLaunchWithMicOpen(bool launch_with_mic_open) override;
  void NotifyArcPlayStoreEnabledChanged(bool enabled) override;
  void NotifyLockedFullScreenStateChanged(bool enabled) override;
  void AddObserver(ash::mojom::VoiceInteractionObserverPtr observer) override {}

  ash::mojom::VoiceInteractionState voice_interaction_state() const {
    return voice_interaction_state_;
  }
  bool voice_interaction_settings_enabled() const {
    return voice_interaction_settings_enabled_;
  }
  bool voice_interaction_context_enabled() const {
    return voice_interaction_context_enabled_;
  }
  bool voice_interaction_hotword_enabled() const {
    return voice_interaction_hotword_enabled_;
  }
  ash::mojom::ConsentStatus voice_interaction_consent_status() const {
    return consent_status_;
  }
  ash::mojom::AssistantAllowedState assistant_allowed_state() const {
    return assistant_allowed_state_;
  }
  bool voice_interaction_notification_enabled() const {
    return voice_interaction_notification_enabled_;
  }
  const std::string& locale() const { return locale_; }
  bool launch_with_mic_open() const { return launch_with_mic_open_; }
  bool arc_play_store_enabled() const { return arc_play_store_enabled_; }
  bool locked_full_screen_enabled() const {
    return locked_full_screen_enabled_;
  }

 private:
  ash::mojom::VoiceInteractionState voice_interaction_state_ =
      ash::mojom::VoiceInteractionState::STOPPED;
  bool voice_interaction_settings_enabled_ = false;
  bool voice_interaction_context_enabled_ = false;
  bool voice_interaction_hotword_always_on_ = false;
  bool voice_interaction_hotword_enabled_ = false;
  ash::mojom::ConsentStatus consent_status_ =
      ash::mojom::ConsentStatus::kUnknown;
  bool voice_interaction_notification_enabled_ = false;
  std::string locale_;
  ash::mojom::AssistantAllowedState assistant_allowed_state_ =
      ash::mojom::AssistantAllowedState::DISALLOWED_BY_INCOGNITO;
  bool launch_with_mic_open_ = false;
  bool arc_play_store_enabled_ = false;
  bool locked_full_screen_enabled_ = false;

  mojo::Binding<ash::mojom::VoiceInteractionController> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeVoiceInteractionController);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_FAKE_VOICE_INTERACTION_CONTROLLER_H_
