// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/assistant/assistant_client.h"

#include <utility>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"
#include "chrome/browser/chromeos/assistant/assistant_card_renderer.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

namespace {
// Owned by ChromeBrowserMainChromeOS:
AssistantClient* g_instance = nullptr;
}  // namespace

// static
AssistantClient* AssistantClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

AssistantClient::AssistantClient()
    : client_binding_(this), audio_input_binding_(&audio_input_) {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantClient::~AssistantClient() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

void AssistantClient::Start(service_manager::Connector* connector) {
  connector->BindInterface(mojom::kServiceName, &assistant_connection_);
  mojom::AudioInputPtr audio_input_ptr;
  audio_input_binding_.Bind(mojo::MakeRequest(&audio_input_ptr));

  mojom::ClientPtr client_ptr;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr));

  assistant_connection_->Init(std::move(client_ptr),
                              std::move(audio_input_ptr));

  assistant_card_renderer_.reset(new AssistantCardRenderer(connector));
}

void AssistantClient::OnAssistantStatusChanged(bool running) {
  arc::VoiceInteractionControllerClient::Get()->NotifyStatusChanged(
      running ? ash::mojom::VoiceInteractionState::RUNNING
              : ash::mojom::VoiceInteractionState::STOPPED);
}

}  // namespace assistant
}  // namespace chromeos
