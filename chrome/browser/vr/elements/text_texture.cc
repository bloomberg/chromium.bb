// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text_texture.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"

namespace vr {

TextTexture::TextTexture(float font_height) : font_height_(font_height) {}

TextTexture::~TextTexture() {}

void TextTexture::SetText(const base::string16& text) {
  SetAndDirty(&text_, text);
}

void TextTexture::SetColor(SkColor color) {
  SetAndDirty(&color_, color);
}

void TextTexture::SetAlignment(TextAlignment alignment) {
  SetAndDirty(&alignment_, alignment);
}

void TextTexture::SetMultiLine(bool multiline) {
  SetAndDirty(&multiline_, multiline);
}

void TextTexture::SetTextWidth(float width) {
  SetAndDirty(&text_width_, width);
}

gfx::SizeF TextTexture::GetDrawnSize() const {
  return size_;
}

std::vector<std::unique_ptr<gfx::RenderText>> TextTexture::LayOutText(
    const gfx::Size& texture_size) {
  gfx::FontList fonts;
  float pixels_per_meter = texture_size.width() / text_width_;
  int pixel_font_height = static_cast<int>(font_height_ * pixels_per_meter);
  GetDefaultFontList(pixel_font_height, text_, &fonts);
  gfx::Rect text_bounds(texture_size.width(), 0);

  std::vector<std::unique_ptr<gfx::RenderText>> lines =
      // TODO(vollick): if this subsumes all text, then we should probably move
      // this function into this class.
      PrepareDrawStringRect(
          text_, fonts, color_, &text_bounds, alignment_,
          multiline_ ? kWrappingBehaviorWrap : kWrappingBehaviorNoWrap);

  // Note, there is no padding here whatsoever.
  size_ = gfx::SizeF(text_bounds.size());

  return lines;
}

gfx::Size TextTexture::GetPreferredTextureSize(int width) const {
  return gfx::Size(width, width);
}

void TextTexture::Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  auto lines = LayOutText(texture_size);
  for (auto& render_text : lines)
    render_text->Draw(canvas);
}

}  // namespace vr
