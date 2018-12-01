// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace chromeos {
namespace assistant {

class AssistantAudioDecoderService : public service_manager::Service {
 public:
  explicit AssistantAudioDecoderService(
      service_manager::mojom::ServiceRequest request);
  ~AssistantAudioDecoderService() override;

 private:
  // service_manager::Service overrides:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive service_keepalive_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAudioDecoderService);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_SERVICE_H_
