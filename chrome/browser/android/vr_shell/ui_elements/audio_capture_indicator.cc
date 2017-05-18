// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/audio_capture_indicator.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/system_indicator_texture.h"
#include "chrome/grit/generated_resources.h"
#include "ui/vector_icons/vector_icons.h"

namespace vr_shell {

AudioCaptureIndicator::AudioCaptureIndicator(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<SystemIndicatorTexture>(
          ui::kMicrophoneIcon,
          IDS_AUDIO_CALL_NOTIFICATION_TEXT_2)) {}

AudioCaptureIndicator::~AudioCaptureIndicator() = default;

UiTexture* AudioCaptureIndicator::GetTexture() const {
  return texture_.get();
}

}  // namespace vr_shell
