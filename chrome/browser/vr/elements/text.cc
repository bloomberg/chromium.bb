// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"

namespace {
constexpr float kCursorWidthRatio = 0.07f;
}

namespace vr {

class TextTexture : public UiTexture {
 public:
  explicit TextTexture(float font_height) : font_height_(font_height) {}
  ~TextTexture() override {}

  void SetText(const base::string16& text) { SetAndDirty(&text_, text); }

  void SetColor(SkColor color) { SetAndDirty(&color_, color); }

  void SetAlignment(TextAlignment alignment) {
    SetAndDirty(&alignment_, alignment);
  }

  void SetMultiLine(bool multiline) { SetAndDirty(&multiline_, multiline); }
  void SetCursorEnabled(bool enabled) {
    SetAndDirty(&cursor_enabled_, enabled);
  }

  void SetCursorPosition(int position) {
    SetAndDirty(&cursor_position_, position);
  }

  void SetTextWidth(float width) { SetAndDirty(&text_width_, width); }

  gfx::SizeF GetDrawnSize() const override { return size_; }

  gfx::Rect get_cursor_bounds() { return cursor_bounds_; }

  // This method does all text preparation for the element other than drawing to
  // the texture. This allows for deeper unit testing of the Text element
  // without having to mock canvases and simulate frame rendering. The state of
  // the texture is modified here.
  std::vector<std::unique_ptr<gfx::RenderText>> LayOutText(
      const gfx::Size& texture_size);

 private:
  gfx::Size GetPreferredTextureSize(int width) const override {
    return gfx::Size(width, width);
  }

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  base::string16 text_;
  // These dimensions are in meters.
  float font_height_ = 0;
  float text_width_ = 0;
  TextAlignment alignment_ = kTextAlignmentCenter;
  bool multiline_ = true;
  SkColor color_ = SK_ColorBLACK;
  bool cursor_enabled_ = false;
  int cursor_position_ = 0;
  gfx::Rect cursor_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TextTexture);
};

Text::Text(int maximum_width_pixels, float font_height_meters)
    : TexturedElement(maximum_width_pixels),
      texture_(base::MakeUnique<TextTexture>(font_height_meters)) {}
Text::~Text() {}

void Text::SetText(const base::string16& text) {
  texture_->SetText(text);
}

void Text::SetColor(SkColor color) {
  texture_->SetColor(color);
}

void Text::SetTextAlignment(UiTexture::TextAlignment alignment) {
  texture_->SetAlignment(alignment);
}

void Text::SetMultiLine(bool multiline) {
  texture_->SetMultiLine(multiline);
}

void Text::SetCursorEnabled(bool enabled) {
  texture_->SetCursorEnabled(enabled);
}

void Text::SetCursorPosition(int position) {
  texture_->SetCursorPosition(position);
}

gfx::Rect Text::GetRawCursorBounds() const {
  return texture_->get_cursor_bounds();
}

gfx::RectF Text::GetCursorBounds() const {
  // Note that gfx:: cursor bounds always indicate a one-pixel width, so we
  // override the width here to be a percentage of height for the sake of
  // arbitrary texture sizes.
  gfx::Rect bounds = texture_->get_cursor_bounds();
  float scale = size().width() / texture_->GetDrawnSize().width();
  return gfx::RectF(
      bounds.CenterPoint().x() * scale, bounds.CenterPoint().y() * scale,
      bounds.height() * scale * kCursorWidthRatio, bounds.height() * scale);
}

void Text::OnSetSize(const gfx::SizeF& size) {
  texture_->SetTextWidth(size.width());
}

std::vector<std::unique_ptr<gfx::RenderText>> Text::LayOutTextForTest(
    const gfx::Size& texture_size) {
  return texture_->LayOutText(texture_size);
}

gfx::SizeF Text::GetTextureSizeForTest() const {
  return texture_->GetDrawnSize();
}

UiTexture* Text::GetTexture() const {
  return texture_.get();
}

std::vector<std::unique_ptr<gfx::RenderText>> TextTexture::LayOutText(
    const gfx::Size& texture_size) {
  gfx::FontList fonts;
  float pixels_per_meter = texture_size.width() / text_width_;
  int pixel_font_height = static_cast<int>(font_height_ * pixels_per_meter);
  GetDefaultFontList(pixel_font_height, text_, &fonts);
  gfx::Rect text_bounds(texture_size.width(), 0);

  TextRenderParameters parameters;
  parameters.color = color_;
  parameters.text_alignment = alignment_;
  parameters.wrapping_behavior =
      multiline_ ? kWrappingBehaviorWrap : kWrappingBehaviorNoWrap;
  parameters.cursor_enabled = cursor_enabled_;
  parameters.cursor_position = cursor_position_;

  std::vector<std::unique_ptr<gfx::RenderText>> lines =
      // TODO(vollick): if this subsumes all text, then we should probably move
      // this function into this class.
      PrepareDrawStringRect(text_, fonts, &text_bounds, parameters);

  if (cursor_enabled_)
    cursor_bounds_ = lines.front()->GetUpdatedCursorBounds();

  // Note, there is no padding here whatsoever.
  size_ = gfx::SizeF(text_bounds.size());

  return lines;
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
