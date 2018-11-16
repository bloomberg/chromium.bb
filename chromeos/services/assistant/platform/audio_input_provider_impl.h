// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/assistant/platform/audio_input_impl.h"
#include "libassistant/shared/public/platform_audio_input.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

class AudioInputProviderImpl : public assistant_client::AudioInputProvider {
 public:
  explicit AudioInputProviderImpl(service_manager::Connector* connector);
  ~AudioInputProviderImpl() override;

  // assistant_client::AudioInputProvider overrides:
  AudioInputImpl& GetAudioInput() override;
  int64_t GetCurrentAudioTime() override;

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open);

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

 private:
  AudioInputImpl audio_input_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_
