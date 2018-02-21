// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/exit_prompt_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

class AudioPermissionPromptTexture : public ExitPromptTexture {
 public:
  AudioPermissionPromptTexture();
  ~AudioPermissionPromptTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;

  void SetIconColor(SkColor color);

 private:
  void SetContentMessageId(int message_id) override;
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  SkColor icon_color_ = SK_ColorBLACK;

  DISALLOW_COPY_AND_ASSIGN(AudioPermissionPromptTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_TEXTURE_H_
