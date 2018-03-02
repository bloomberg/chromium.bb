// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/prompt.h"

#include "chrome/browser/vr/elements/prompt_texture.h"

namespace vr {

Prompt::Prompt(int preferred_width,
               int content_message_id,
               const gfx::VectorIcon& icon,
               int primary_button_message_id,
               int secondary_button_message_id,
               const ExitPromptCallback& result_callback)
    : ExitPrompt(preferred_width,
                 result_callback,
                 std::make_unique<PromptTexture>(content_message_id,
                                                 icon,
                                                 primary_button_message_id,
                                                 secondary_button_message_id)) {
  set_reason(UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
}

Prompt::~Prompt() = default;

void Prompt::SetIconColor(SkColor color) {
  static_cast<PromptTexture*>(GetTexture())->SetIconColor(color);
}

}  // namespace vr
