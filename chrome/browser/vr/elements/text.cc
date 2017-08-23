// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"

namespace vr {

class TextTexture : public UiTexture {
 public:
  TextTexture(int resource_id,
              float font_height,
              float text_width,
              const base::Callback<SkColor(ColorScheme)>& color_callback)
      : resource_id_(resource_id),
        font_height_(font_height),
        text_width_(text_width),
        color_callback_(color_callback) {}

  ~TextTexture() override {}

  void OnSetMode() override { set_dirty(); }

 private:
  gfx::Size GetPreferredTextureSize(int width) const override {
    return gfx::Size(width, width);
  }

  gfx::SizeF GetDrawnSize() const override { return size_; }

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  int resource_id_;
  // These widths are in meters.
  float font_height_;
  float text_width_;

  base::Callback<SkColor(ColorScheme)> color_callback_;

  DISALLOW_COPY_AND_ASSIGN(TextTexture);
};

Text::Text(int maximum_width_pixels,
           float font_height_meters,
           float text_width_meters,
           const base::Callback<SkColor(ColorScheme)>& color_callback,
           int resource_id)
    : TexturedElement(maximum_width_pixels),
      texture_(base::MakeUnique<TextTexture>(resource_id,
                                             font_height_meters,
                                             text_width_meters,
                                             color_callback)) {}
Text::~Text() {}

UiTexture* Text::GetTexture() const {
  return texture_.get();
}

void TextTexture::Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  gfx::FontList fonts;
  auto text = l10n_util::GetStringUTF16(resource_id_);
  float pixels_per_meter = texture_size.width() / text_width_;
  int pixel_font_height = static_cast<int>(font_height_ * pixels_per_meter);
  GetFontList(pixel_font_height, text, &fonts);
  gfx::Rect text_bounds(texture_size.width(), 0);

  std::vector<std::unique_ptr<gfx::RenderText>> lines =
      // TODO(vollick): if this subsumes all text, then we should probably move
      // this function into this class.
      PrepareDrawStringRect(text, fonts, color_callback_.Run(color_scheme()),
                            &text_bounds, kTextAlignmentCenter,
                            kWrappingBehaviorWrap);
  // Draw the text.
  for (auto& render_text : lines)
    render_text->Draw(canvas);

  // Note, there is no padding here whatsoever.
  size_ = gfx::SizeF(text_bounds.size());
}

}  // namespace vr
