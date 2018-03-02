// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_PROMPT_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_PROMPT_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/exit_prompt_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class PromptTexture : public ExitPromptTexture {
 public:
  PromptTexture(int content_message_id,
                const gfx::VectorIcon& icon,
                int primary_button_message_id,
                int secondary_button_message_id);
  ~PromptTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;

  void SetIconColor(SkColor color);

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  SkColor icon_color_ = SK_ColorBLACK;
  const gfx::VectorIcon& icon_;
  int primary_button_message_id_ = -1;
  int secondary_button_message_id_ = -1;

  DISALLOW_COPY_AND_ASSIGN(PromptTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_PROMPT_TEXTURE_H_
