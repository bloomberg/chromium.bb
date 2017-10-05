// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_TEXTURE_H_

#include "chrome/browser/vr/elements/ui_texture.h"

namespace vr {

class TextTexture : public UiTexture {
 public:
  TextTexture(const base::string16& text, float font_height, float text_width);

  ~TextTexture() override;

  void SetColor(SkColor color);

 private:
  gfx::Size GetPreferredTextureSize(int width) const override;

  gfx::SizeF GetDrawnSize() const override;

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  base::string16 text_;
  float font_height_meters_;
  float text_width_meters_;

  SkColor color_ = SK_ColorBLACK;

  DISALLOW_COPY_AND_ASSIGN(TextTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_TEXTURE_H_
