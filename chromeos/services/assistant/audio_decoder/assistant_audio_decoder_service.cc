// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_service.h"

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_factory.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chromeos {
namespace assistant {

namespace {

void OnAudioDecoderFactoryRequest(
    service_manager::ServiceKeepalive* keepalive,
    mojom::AssistantAudioDecoderFactoryRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<AssistantAudioDecoderFactory>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

AssistantAudioDecoderService::AssistantAudioDecoderService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

AssistantAudioDecoderService::~AssistantAudioDecoderService() = default;

void AssistantAudioDecoderService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&OnAudioDecoderFactoryRequest, &service_keepalive_));
}

void AssistantAudioDecoderService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace assistant
}  // namespace chromeos
