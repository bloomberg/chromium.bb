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

namespace vr {

namespace {

constexpr float kCursorWidthRatio = 0.07f;
constexpr int kTextPixelPerDmm = 1100;

int DmmToPixel(float dmm) {
  return static_cast<int>(dmm * kTextPixelPerDmm);
}

float PixelToDmm(int pixel) {
  return static_cast<float>(pixel) / kTextPixelPerDmm;
}

bool IsFixedWidthLayout(TextLayoutMode mode) {
  return mode == kSingleLineFixedWidth || mode == kMultiLineFixedWidth;
}

}  // namespace

class TextTexture : public UiTexture {
 public:
  TextTexture() = default;

  ~TextTexture() override {}

  void SetFontHeightInDmm(float font_height_dmms) {
    SetAndDirty(&font_height_dmms_, font_height_dmms);
  }

  void SetText(const base::string16& text) { SetAndDirty(&text_, text); }

  void SetColor(SkColor color) { SetAndDirty(&color_, color); }

  void SetAlignment(TextAlignment alignment) {
    SetAndDirty(&alignment_, alignment);
  }

  void SetTextLayoutMode(TextLayoutMode mode) {
    SetAndDirty(&text_layout_mode_, mode);
  }

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
  void LayOutText();

  const std::vector<std::unique_ptr<gfx::RenderText>>& lines() {
    return lines_;
  }

 private:
  void OnMeasureSize() override { LayOutText(); }

  gfx::Size GetPreferredTextureSize(int width) const override {
    return gfx::Size(GetDrawnSize().width(), GetDrawnSize().height());
  }

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  base::string16 text_;
  float font_height_dmms_ = 0;
  float text_width_ = 0;
  TextAlignment alignment_ = kTextAlignmentCenter;
  TextLayoutMode text_layout_mode_ = kMultiLineFixedWidth;
  SkColor color_ = SK_ColorBLACK;
  bool cursor_enabled_ = false;
  int cursor_position_ = 0;
  gfx::Rect cursor_bounds_;
  std::vector<std::unique_ptr<gfx::RenderText>> lines_;

  DISALLOW_COPY_AND_ASSIGN(TextTexture);
};

Text::Text(float font_height_dmms)
    : TexturedElement(0), texture_(base::MakeUnique<TextTexture>()) {
  texture_->SetFontHeightInDmm(font_height_dmms);
}

Text::~Text() {}

void Text::SetFontHeightInDmm(float font_height_dmms) {
  texture_->SetFontHeightInDmm(font_height_dmms);
}

void Text::SetText(const base::string16& text) {
  texture_->SetText(text);
}

void Text::SetColor(SkColor color) {
  texture_->SetColor(color);
}

void Text::SetTextAlignment(UiTexture::TextAlignment alignment) {
  texture_->SetAlignment(alignment);
}

void Text::SetTextLayoutMode(TextLayoutMode mode) {
  text_layout_mode_ = mode;
  texture_->SetTextLayoutMode(mode);
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
  if (IsFixedWidthLayout(text_layout_mode_))
    texture_->SetTextWidth(size.width());
}

void Text::UpdateElementSize() {
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  // Width calculated from PixelToDmm may be different from the width saved in
  // stale_size due to float percision. So use the value in stale_size for fixed
  // width text layout.
  float width = IsFixedWidthLayout(text_layout_mode_)
                    ? stale_size().width()
                    : PixelToDmm(drawn_size.width());
  SetSize(width, PixelToDmm(drawn_size.height()));
}

const std::vector<std::unique_ptr<gfx::RenderText>>& Text::LayOutTextForTest() {
  texture_->LayOutText();
  return texture_->lines();
}

gfx::SizeF Text::GetTextureSizeForTest() const {
  return texture_->GetDrawnSize();
}

UiTexture* Text::GetTexture() const {
  return texture_.get();
}

void TextTexture::LayOutText() {
  gfx::FontList fonts;
  int pixel_font_height = DmmToPixel(font_height_dmms_);
  GetDefaultFontList(pixel_font_height, text_, &fonts);
  gfx::Rect text_bounds;
  if (text_layout_mode_ == kSingleLineFixedHeight) {
    text_bounds.set_height(pixel_font_height);
  } else {
    text_bounds.set_width(DmmToPixel(text_width_));
  }

  TextRenderParameters parameters;
  parameters.color = color_;
  parameters.text_alignment = alignment_;
  parameters.wrapping_behavior = text_layout_mode_ == kMultiLineFixedWidth
                                     ? kWrappingBehaviorWrap
                                     : kWrappingBehaviorNoWrap;
  parameters.cursor_enabled = cursor_enabled_;
  parameters.cursor_position = cursor_position_;

  lines_ =
      // TODO(vollick): if this subsumes all text, then we should probably move
      // this function into this class.
      PrepareDrawStringRect(text_, fonts, &text_bounds, parameters);

  if (cursor_enabled_)
    cursor_bounds_ = lines_.front()->GetUpdatedCursorBounds();

  // Note, there is no padding here whatsoever.
  size_ = gfx::SizeF(text_bounds.size());
}

void TextTexture::Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  for (auto& render_text : lines_)
    render_text->Draw(canvas);
}

}  // namespace vr
