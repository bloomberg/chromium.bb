// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_a11y_controller.h"

namespace chromeos {

AudioA11yController::AudioA11yController() : cras_audio_handler_(nullptr) {
  cras_audio_handler_ = CrasAudioHandler::Get();
}

AudioA11yController::~AudioA11yController() {
}

void AudioA11yController::SetOutputMono(bool enabled) {
  if (cras_audio_handler_->IsOutputMonoEnabled() == enabled)
    return;

  cras_audio_handler_->SetOutputMono(enabled);
}

}  // namespace chromeos




