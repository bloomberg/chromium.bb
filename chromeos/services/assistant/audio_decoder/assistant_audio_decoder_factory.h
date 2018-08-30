// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_FACTORY_H_
#define CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_FACTORY_H_

#include <memory>

#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace chromeos {
namespace assistant {

class AssistantAudioDecoderFactory
    : public mojom::AssistantAudioDecoderFactory {
 public:
  explicit AssistantAudioDecoderFactory(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~AssistantAudioDecoderFactory() override;

 private:
  // mojom::AssistantAudioDecoderFactory:
  void CreateAssistantAudioDecoder(
      mojom::AssistantAudioDecoderRequest request,
      mojom::AssistantAudioDecoderClientPtr client,
      mojom::AssistantMediaDataSourcePtr data_source) override;

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAudioDecoderFactory);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_FACTORY_H_
