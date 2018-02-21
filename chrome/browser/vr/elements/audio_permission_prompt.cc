// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/audio_permission_prompt.h"

#include "chrome/browser/vr/elements/audio_permission_prompt_texture.h"

namespace vr {

AudioPermissionPrompt::AudioPermissionPrompt(
    int preferred_width,
    const ExitPromptCallback& result_callback)
    : ExitPrompt(preferred_width,
                 result_callback,
                 std::make_unique<AudioPermissionPromptTexture>()) {
  set_reason(UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
}

AudioPermissionPrompt::~AudioPermissionPrompt() = default;

void AudioPermissionPrompt::SetIconColor(SkColor color) {
  static_cast<AudioPermissionPromptTexture*>(GetTexture())->SetIconColor(color);
}

}  // namespace vr
