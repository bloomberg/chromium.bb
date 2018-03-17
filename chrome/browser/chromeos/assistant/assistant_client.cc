// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/assistant/assistant_client.h"

#include <utility>

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

AssistantClient::AssistantClient() : audio_input_binding_(&audio_input_) {
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

  assistant_connection_->Init(std::move(audio_input_ptr));
}

}  // namespace assistant
}  // namespace chromeos
