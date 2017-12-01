// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_TEXTURE_H_

#include <memory>

#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class RenderText;
}

namespace vr {

class TextTexture : public UiTexture {
 public:
  explicit TextTexture(float font_height);
  ~TextTexture() override;

  void SetText(const base::string16& text);
  void SetColor(SkColor color);
  void SetAlignment(TextAlignment alignment);
  void SetMultiLine(bool multiline);
  void SetTextWidth(float width);

  gfx::SizeF GetDrawnSize() const override;

  // This method does all text preparation for the element other than drawing to
  // the texture. This allows for deeper unit testing of the Text element
  // without having to mock canvases and simulate frame rendering. The state of
  // the texture is modified here.
  std::vector<std::unique_ptr<gfx::RenderText>> LayOutText(
      const gfx::Size& texture_size);

 private:
  gfx::Size GetPreferredTextureSize(int width) const override;
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  base::string16 text_;
  // These dimensions are in meters.
  float font_height_ = 0;
  float text_width_ = 0;
  TextAlignment alignment_ = kTextAlignmentCenter;
  bool multiline_ = true;
  SkColor color_ = SK_ColorBLACK;

  DISALLOW_COPY_AND_ASSIGN(TextTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_TEXTURE_H_
