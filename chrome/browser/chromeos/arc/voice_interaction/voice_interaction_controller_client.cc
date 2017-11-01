// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {

VoiceInteractionControllerClient::VoiceInteractionControllerClient() {
  ConnectToVoiceInteractionController();
}

VoiceInteractionControllerClient::~VoiceInteractionControllerClient() = default;

void VoiceInteractionControllerClient::NotifyStatusChanged(
    ash::mojom::VoiceInteractionState state) {
  DCHECK(voice_interaction_controller_);
  voice_interaction_controller_->NotifyStatusChanged(state);
}

void VoiceInteractionControllerClient::NotifySettingsEnabled(bool enabled) {
  DCHECK(voice_interaction_controller_);
  voice_interaction_controller_->NotifySettingsEnabled(enabled);
}

void VoiceInteractionControllerClient::NotifyContextEnabled(bool enabled) {
  DCHECK(voice_interaction_controller_);
  voice_interaction_controller_->NotifyContextEnabled(enabled);
}

void VoiceInteractionControllerClient::NotifySetupCompleted(bool completed) {
  DCHECK(voice_interaction_controller_);
  voice_interaction_controller_->NotifySetupCompleted(completed);
}

void VoiceInteractionControllerClient::SetControllerForTesting(
    ash::mojom::VoiceInteractionControllerPtr controller) {
  voice_interaction_controller_ = std::move(controller);
}

void VoiceInteractionControllerClient::FlushMojoForTesting() {
  voice_interaction_controller_.FlushForTesting();
}

void VoiceInteractionControllerClient::ConnectToVoiceInteractionController() {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  // Tests may bind to their own VoiceInteractionController later.
  if (connection)
    connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                              &voice_interaction_controller_);
}

}  // namespace arc
