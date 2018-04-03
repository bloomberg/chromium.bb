// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

constexpr float kCursorWidthRatio = 0.07f;
constexpr int kTextPixelPerDmm = 1100;
constexpr float kTextShadowScaleFactor = 1000.0f;

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

TextFormattingAttribute::TextFormattingAttribute(SkColor color,
                                                 const gfx::Range& range)
    : type_(COLOR), range_(range), color_(color) {}

TextFormattingAttribute::TextFormattingAttribute(gfx::Font::Weight weight,
                                                 const gfx::Range& range)
    : type_(WEIGHT), range_(range), weight_(weight) {}

TextFormattingAttribute::TextFormattingAttribute(
    gfx::DirectionalityMode directionality)
    : type_(DIRECTIONALITY), directionality_(directionality) {}

bool TextFormattingAttribute::operator==(
    const TextFormattingAttribute& other) const {
  if (type_ != other.type_ || range_ != other.range_)
    return false;
  switch (type_) {
    case COLOR:
      return color_ == other.color_;
    case WEIGHT:
      return weight_ == other.weight_;
    case DIRECTIONALITY:
      return directionality_ == other.directionality_;
    default:
      NOTREACHED();
      return false;
  }
}

bool TextFormattingAttribute::operator!=(
    const TextFormattingAttribute& other) const {
  return !(*this == other);
}

void TextFormattingAttribute::Apply(RenderTextWrapper* render_text) const {
  switch (type_) {
    case COLOR: {
      if (range_.IsValid()) {
        render_text->ApplyColor(color_, range_);
      } else {
        render_text->SetColor(color_);
      }
      break;
    }
    case WEIGHT:
      if (range_.IsValid()) {
        render_text->ApplyWeight(weight_, range_);
      } else {
        render_text->SetWeight(weight_);
      }
      break;
    case DIRECTIONALITY:
      render_text->SetDirectionalityMode(directionality_);
      break;
    default:
      NOTREACHED();
  }
}

class TextTexture : public UiTexture {
 public:
  TextTexture() = default;

  ~TextTexture() override {}

  void SetFontHeightInDmm(float font_height_dmms) {
    SetAndDirty(&font_height_dmms_, font_height_dmms);
  }

  void SetText(const base::string16& text) { SetAndDirty(&text_, text); }

  void SetColor(SkColor color) { SetAndDirty(&color_, color); }

  void SetSelectionColors(const TextSelectionColors& colors) {
    SetAndDirty(&selection_colors_, colors);
  }

  void SetFormatting(const TextFormatting& formatting) {
    SetAndDirty(&formatting_, formatting);
  }

  void SetAlignment(TextAlignment alignment) {
    SetAndDirty(&alignment_, alignment);
  }

  void SetLayoutMode(TextLayoutMode mode) {
    SetAndDirty(&text_layout_mode_, mode);
  }

  void SetCursorEnabled(bool enabled) {
    SetAndDirty(&cursor_enabled_, enabled);
  }

  void SetSelectionIndices(int start, int end) {
    SetAndDirty(&selection_start_, start);
    SetAndDirty(&selection_end_, end);
  }

  int GetCursorPositionFromPoint(const gfx::PointF& point) const {
    DCHECK_EQ(lines().size(), 1u);
    gfx::Point pixel_position(point.x() * GetDrawnSize().width(),
                              point.y() * GetDrawnSize().height());
    return lines().front()->FindCursorPosition(pixel_position).caret_pos();
  }

  void SetShadowsEnabled(bool enabled) {
    SetAndDirty(&shadows_enabled_, enabled);
  }

  void SetTextWidth(float width) { SetAndDirty(&text_width_, width); }

  gfx::SizeF GetDrawnSize() const override { return size_; }

  gfx::Rect get_cursor_bounds() { return cursor_bounds_; }

  // This method does all text preparation for the element other than drawing to
  // the texture. This allows for deeper unit testing of the Text element
  // without having to mock canvases and simulate frame rendering. The state of
  // the texture is modified here.
  void LayOutText();

  const std::vector<std::unique_ptr<gfx::RenderText>>& lines() const {
    return lines_;
  }

  void SetOnUnhandledCodePointCallback(
      base::RepeatingCallback<void()> callback) {
    unhandled_codepoint_callback_ = callback;
  }

  void SetOnRenderTextCreated(
      base::RepeatingCallback<void(gfx::RenderText*)> callback) {
    render_text_created_callback_ = callback;
  }

  void SetOnRenderTextRendered(
      base::RepeatingCallback<void(const gfx::RenderText&, SkCanvas* canvas)>
          callback) {
    render_text_rendered_callback_ = callback;
  }

  void set_unsupported_code_points_for_test(bool unsupported) {
    unsupported_code_point_for_test_ = unsupported;
  }

 private:
  void OnMeasureSize() override { LayOutText(); }

  gfx::Size GetPreferredTextureSize(int width) const override {
    return gfx::Size(GetDrawnSize().width(), GetDrawnSize().height());
  }

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  gfx::Vector2d texture_offset_;
  base::string16 text_;
  float font_height_dmms_ = 0;
  float text_width_ = 0;
  TextAlignment alignment_ = kTextAlignmentCenter;
  TextLayoutMode text_layout_mode_ = kMultiLineFixedWidth;
  SkColor color_ = SK_ColorBLACK;
  TextSelectionColors selection_colors_;
  TextFormatting formatting_;
  bool cursor_enabled_ = false;
  int selection_start_ = 0;
  int selection_end_ = 0;
  gfx::Rect cursor_bounds_;
  bool shadows_enabled_ = false;
  std::vector<std::unique_ptr<gfx::RenderText>> lines_;

  base::RepeatingCallback<void()> unhandled_codepoint_callback_;
  base::RepeatingCallback<void(gfx::RenderText*)> render_text_created_callback_;
  base::RepeatingCallback<void(const gfx::RenderText&, SkCanvas*)>
      render_text_rendered_callback_;

  bool unsupported_code_point_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(TextTexture);
};

Text::Text(float font_height_dmms)
    : TexturedElement(0), texture_(std::make_unique<TextTexture>()) {
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

void Text::SetSelectionColors(const TextSelectionColors& colors) {
  texture_->SetSelectionColors(colors);
}

void Text::SetFormatting(const TextFormatting& formatting) {
  texture_->SetFormatting(formatting);
}

void Text::SetAlignment(UiTexture::TextAlignment alignment) {
  texture_->SetAlignment(alignment);
}

void Text::SetLayoutMode(TextLayoutMode mode) {
  text_layout_mode_ = mode;
  texture_->SetLayoutMode(mode);
}

void Text::SetCursorEnabled(bool enabled) {
  texture_->SetCursorEnabled(enabled);
}

void Text::SetSelectionIndices(int start, int end) {
  texture_->SetSelectionIndices(start, end);
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

int Text::GetCursorPositionFromPoint(const gfx::PointF& point) const {
  return texture_->GetCursorPositionFromPoint(point);
}

void Text::SetShadowsEnabled(bool enabled) {
  texture_->SetShadowsEnabled(enabled);
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

void Text::SetUnsupportedCodePointsForTest(bool unsupported) {
  texture_->set_unsupported_code_points_for_test(unsupported);
}

void Text::SetOnUnhandledCodePointCallback(
    base::RepeatingCallback<void()> callback) {
  texture_->SetOnUnhandledCodePointCallback(callback);
}

void Text::SetOnRenderTextCreated(
    base::RepeatingCallback<void(gfx::RenderText*)> callback) {
  texture_->SetOnRenderTextCreated(callback);
}

void Text::SetOnRenderTextRendered(
    base::RepeatingCallback<void(const gfx::RenderText&, SkCanvas* canvas)>
        callback) {
  texture_->SetOnRenderTextRendered(callback);
}

float Text::MetersToPixels(float meters) {
  return DmmToPixel(meters);
}

UiTexture* Text::GetTexture() const {
  return texture_.get();
}

void TextTexture::LayOutText() {
  int pixel_font_height = DmmToPixel(font_height_dmms_);
  gfx::Rect text_bounds;
  if (text_layout_mode_ == kSingleLineFixedWidth ||
      text_layout_mode_ == kMultiLineFixedWidth) {
    text_bounds.set_width(DmmToPixel(text_width_));
  }

  gfx::FontList fonts;
  if (!GetDefaultFontList(pixel_font_height, text_, &fonts) ||
      unsupported_code_point_for_test_) {
    if (unhandled_codepoint_callback_)
      unhandled_codepoint_callback_.Run();
  }

  TextRenderParameters parameters;
  parameters.color = color_;
  parameters.text_alignment = alignment_;
  parameters.wrapping_behavior = text_layout_mode_ == kMultiLineFixedWidth
                                     ? kWrappingBehaviorWrap
                                     : kWrappingBehaviorNoWrap;
  parameters.cursor_enabled = cursor_enabled_;
  parameters.cursor_position = selection_end_;
  parameters.shadows_enabled = shadows_enabled_;
  parameters.shadow_size = kTextShadowScaleFactor * font_height_dmms_;

  lines_ =
      // TODO(vollick): if this subsumes all text, then we should probably move
      // this function into this class.
      PrepareDrawStringRect(text_, fonts, &text_bounds, parameters);

  if (cursor_enabled_) {
    DCHECK_EQ(lines_.size(), 1u);
    gfx::RenderText* render_text = lines_.front().get();

    if (selection_start_ != selection_end_) {
      render_text->set_focused(true);
      gfx::Range range(selection_start_, selection_end_);
      render_text->SetSelection(gfx::SelectionModel(
          range, gfx::LogicalCursorDirection::CURSOR_FORWARD));
      render_text->set_selection_background_focused_color(
          selection_colors_.background);
      render_text->set_selection_color(selection_colors_.foreground);
    }

    cursor_bounds_ = render_text->GetUpdatedCursorBounds();
  }

  if (!formatting_.empty()) {
    DCHECK_EQ(parameters.wrapping_behavior, kWrappingBehaviorNoWrap);
    DCHECK_EQ(lines_.size(), 1u);
    RenderTextWrapper render_text(lines_.front().get());
    for (const auto& attribute : formatting_) {
      attribute.Apply(&render_text);
    }
  }

  if (render_text_created_callback_) {
    DCHECK_EQ(lines_.size(), 1u);
    render_text_created_callback_.Run(lines_.front().get());
  }

  // Note, there is no padding here whatsoever.
  size_ = gfx::SizeF(text_bounds.size());
  if (parameters.shadows_enabled) {
    texture_offset_ = gfx::Vector2d(gfx::ToFlooredInt(parameters.shadow_size),
                                    gfx::ToFlooredInt(parameters.shadow_size));
  }
}

void TextTexture::Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;
  canvas->Translate(texture_offset_);

  for (auto& render_text : lines_)
    render_text->Draw(canvas);

  if (render_text_rendered_callback_) {
    DCHECK_EQ(lines_.size(), 1u);
    render_text_rendered_callback_.Run(*lines_.front().get(), sk_canvas);
  }
}

}  // namespace vr
