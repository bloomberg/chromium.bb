// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text_texture.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"

namespace vr {

TextTexture::TextTexture(const base::string16& text,
                         float font_height,
                         float text_width)
    : text_(text),
      font_height_meters_(font_height),
      text_width_meters_(text_width) {}

TextTexture::~TextTexture() {}

void TextTexture::SetColor(SkColor color) {
  if (color_ == color)
    return;
  color_ = color;
  set_dirty();
}

gfx::Size TextTexture::GetPreferredTextureSize(int width) const {
  return gfx::Size(width, width);
}

gfx::SizeF TextTexture::GetDrawnSize() const {
  return size_;
}

void TextTexture::Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  gfx::FontList fonts;
  float pixels_per_meter = texture_size.width() / text_width_meters_;
  int pixel_font_height =
      static_cast<int>(font_height_meters_ * pixels_per_meter);
  GetFontList(pixel_font_height, text_, &fonts);
  gfx::Rect text_bounds(texture_size.width(), 0);

  std::vector<std::unique_ptr<gfx::RenderText>> lines =
      // TODO(vollick): if this subsumes all text, then we should probably move
      // this function into this class.
      PrepareDrawStringRect(text_, fonts, color_, &text_bounds,
                            kTextAlignmentCenter, kWrappingBehaviorWrap);
  // Draw the text.
  for (auto& render_text : lines)
    render_text->Draw(canvas);

  // Note, there is no padding here whatsoever.
  size_ = gfx::SizeF(text_bounds.size());
}

}  // namespace vr
