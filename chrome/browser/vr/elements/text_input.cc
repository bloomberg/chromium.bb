// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text_input.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/render_text.h"

namespace vr {

class TextInputTexture : public UiTexture {
 public:
  TextInputTexture(float font_height, float text_width)
      : font_height_(font_height), text_width_(text_width) {}
  ~TextInputTexture() override {}

  void SetText(const base::string16& text) { SetAndDirty(&text_, text); }

  void SetCursorPosition(int position) {
    SetAndDirty(&cursor_position_, position);
  }

  void SetColor(SkColor color) { SetAndDirty(&color_, color); }

  void SetCursorVisible(bool visible) {
    SetAndDirty(&cursor_visible_, visible);
  }

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override {
    cc::SkiaPaintCanvas paint_canvas(sk_canvas);
    gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
    gfx::Canvas* canvas = &gfx_canvas;

    gfx::FontList font_list;
    float pixels_per_meter = texture_size.width() / text_width_;
    int pixel_font_height = static_cast<int>(font_height_ * pixels_per_meter);
    GetDefaultFontList(pixel_font_height, text_, &font_list);
    gfx::Rect text_bounds(texture_size.width(), pixel_font_height);
    size_ = gfx::SizeF(text_bounds.size());

    std::unique_ptr<gfx::RenderText> render_text(CreateRenderText());
    render_text->SetText(text_);
    render_text->SetFontList(font_list);
    render_text->SetColor(color_);
    render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    render_text->SetDisplayRect(text_bounds);
    if (cursor_visible_) {
      render_text->SetCursorEnabled(true);
      render_text->SetCursorPosition(cursor_position_);
    }
    render_text->Draw(canvas);

    if (cursor_visible_) {
      auto bounds = render_text->GetUpdatedCursorBounds();
      canvas->DrawRect(gfx::RectF(bounds), 0xFF000080);
    }
  }

 private:
  gfx::Size GetPreferredTextureSize(int width) const override {
    return gfx::Size(width, width * font_height_ / text_width_);
  }

  gfx::SizeF GetDrawnSize() const override { return size_; }

  gfx::SizeF size_;
  base::string16 text_;
  int cursor_position_ = 0;
  float font_height_;
  float text_width_;
  SkColor color_ = SK_ColorBLACK;
  bool cursor_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(TextInputTexture);
};

TextInput::TextInput(int maximum_width_pixels,
                     float font_height_meters,
                     float text_width_meters,
                     OnFocusChangedCallback focus_changed_callback,
                     OnInputEditedCallback input_edit_callback)
    : TexturedElement(maximum_width_pixels),
      texture_(base::MakeUnique<TextInputTexture>(font_height_meters,
                                                  text_width_meters)),
      focus_changed_callback_(focus_changed_callback),
      input_edit_callback_(input_edit_callback) {
  SetSize(text_width_meters, font_height_meters);
}

TextInput::~TextInput() {}

void TextInput::SetTextInputDelegate(TextInputDelegate* text_input_delegate) {
  delegate_ = text_input_delegate;
}

bool TextInput::IsEditable() {
  return true;
}

void TextInput::OnButtonUp(const gfx::PointF& position) {
  if (!delegate_)
    return;

  delegate_->RequestFocus(id());
}

void TextInput::OnFocusChanged(bool focused) {
  focused_ = focused;
  texture_->SetCursorVisible(focused);
  // Update the keyboard with the current text.
  if (delegate_ && focused)
    delegate_->UpdateInput(text_info_);

  focus_changed_callback_.Run(focused);
}

void TextInput::OnInputEdited(const TextInputInfo& info) {
  input_edit_callback_.Run(info);
}

void TextInput::OnInputCommitted(const TextInputInfo& info) {}

void TextInput::SetColor(SkColor color) {
  texture_->SetColor(color);
}

void TextInput::UpdateInput(const TextInputInfo& info) {
  if (text_info_ == info)
    return;

  text_info_ = info;
  texture_->SetText(info.text);
  texture_->SetCursorPosition(info.selection_end);

  if (delegate_ && focused_)
    delegate_->UpdateInput(info);
}

bool TextInput::OnBeginFrame(const base::TimeTicks& time,
                             const gfx::Vector3dF& look_at) {
  base::TimeDelta delta = time - base::TimeTicks();
  if (focused_)
    texture_->SetCursorVisible(delta.InMilliseconds() / 500 % 2);

  return false;
}

UiTexture* TextInput::GetTexture() const {
  return texture_.get();
}

}  // namespace vr
