// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_H_
#define ASH_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class ASH_EXPORT VoiceInteractionController
    : public mojom::VoiceInteractionController,
      public AssistantStateBase {
 public:
  VoiceInteractionController();
  ~VoiceInteractionController() override;

  void BindRequest(mojom::VoiceInteractionControllerRequest request);

  // ash::mojom::VoiceInteractionController:
  void NotifyStatusChanged(mojom::VoiceInteractionState state) override;
  void NotifySettingsEnabled(bool enabled) override;
  void NotifyContextEnabled(bool enabled) override;
  void NotifyHotwordEnabled(bool enabled) override;
  void NotifyHotwordAlwaysOn(bool always_on) override;
  void NotifyConsentStatus(mojom::ConsentStatus consent_status) override;
  void NotifyFeatureAllowed(mojom::AssistantAllowedState state) override;
  void NotifyNotificationEnabled(bool enabled) override;
  void NotifyLocaleChanged(const std::string& locale) override;
  void NotifyLaunchWithMicOpen(bool launch_with_mic_open) override;
  void NotifyArcPlayStoreEnabledChanged(bool enabled) override;
  void NotifyLockedFullScreenStateChanged(bool enabled) override;
  void AddObserver(mojom::VoiceInteractionObserverPtr observer) override;

  // Adding local observers in the same process.
  void AddLocalObserver(DefaultVoiceInteractionObserver* observer);
  void RemoveLocalObserver(DefaultVoiceInteractionObserver* observer);
  void InitObserver(mojom::VoiceInteractionObserver* observer);

  bool notification_enabled() const { return notification_enabled_; }

  bool launch_with_mic_open() const { return launch_with_mic_open_; }

  void FlushForTesting();

 private:
  // Whether notification is enabled.
  bool notification_enabled_ = false;

  // Whether the Assistant should launch with mic open;
  bool launch_with_mic_open_ = false;

  mojo::BindingSet<mojom::VoiceInteractionController> bindings_;

  mojo::InterfacePtrSet<mojom::VoiceInteractionObserver> observers_;

  base::ObserverList<DefaultVoiceInteractionObserver> local_observers_;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionController);
};

}  // namespace ash

#endif  // ASH_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_H_
