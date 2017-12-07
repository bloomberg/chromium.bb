// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text_input.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/geometry/rect.h"

namespace {
constexpr int kCursorBlinkHalfPeriodMs = 600;
}

namespace vr {

TextInput::TextInput(int maximum_width_pixels,
                     float font_height_meters,
                     OnFocusChangedCallback focus_changed_callback,
                     OnInputEditedCallback input_edit_callback)
    : focus_changed_callback_(focus_changed_callback),
      input_edit_callback_(input_edit_callback) {
  auto text = base::MakeUnique<Text>(maximum_width_pixels, font_height_meters);
  text->SetType(kTypeTextInputHint);
  text->SetDrawPhase(kPhaseForeground);
  text->set_hit_testable(false);
  text->set_focusable(false);
  text->set_x_anchoring(LEFT);
  text->set_x_centering(LEFT);
  text->SetSize(1, 1);
  text->SetMultiLine(false);
  text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  hint_element_ = text.get();
  this->AddChild(std::move(text));

  text = base::MakeUnique<Text>(maximum_width_pixels, font_height_meters);
  text->SetType(kTypeTextInputText);
  text->SetDrawPhase(kPhaseForeground);
  text->set_hit_testable(true);
  text->set_focusable(false);
  text->set_x_anchoring(LEFT);
  text->set_x_centering(LEFT);
  text->set_bubble_events(true);
  text->SetSize(1, 1);
  text->SetMultiLine(false);
  text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  text->SetCursorEnabled(true);
  text_element_ = text.get();
  this->AddChild(std::move(text));

  auto cursor = base::MakeUnique<Rect>();
  cursor->SetVisible(false);
  cursor->SetType(kTypeTextInputCursor);
  cursor->SetDrawPhase(kPhaseForeground);
  cursor->set_hit_testable(false);
  cursor->set_focusable(false);
  cursor->set_x_anchoring(LEFT);
  cursor->set_y_anchoring(BOTTOM);
  cursor->SetColor(SK_ColorBLUE);
  cursor_element_ = cursor.get();
  text_element_->AddChild(std::move(cursor));

  set_bounds_contain_children(true);
}

TextInput::~TextInput() {}

void TextInput::SetTextInputDelegate(TextInputDelegate* text_input_delegate) {
  delegate_ = text_input_delegate;
}

void TextInput::OnButtonUp(const gfx::PointF& position) {
  RequestFocus();
}

void TextInput::OnFocusChanged(bool focused) {
  focused_ = focused;

  // Update the keyboard with the current text.
  if (delegate_ && focused)
    delegate_->UpdateInput(text_info_);

  if (focus_changed_callback_)
    focus_changed_callback_.Run(focused);
}

void TextInput::RequestFocus() {
  if (!delegate_)
    return;

  delegate_->RequestFocus(id());
}

void TextInput::SetHintText(const base::string16& text) {
  hint_element_->SetText(text);
}

void TextInput::OnInputEdited(const TextInputInfo& info) {
  input_edit_callback_.Run(info);
}

void TextInput::OnInputCommitted(const TextInputInfo& info) {}

void TextInput::SetTextColor(SkColor color) {
  text_element_->SetColor(color);
}

void TextInput::SetCursorColor(SkColor color) {
  cursor_element_->SetColor(color);
}

void TextInput::SetHintColor(SkColor color) {
  hint_element_->SetColor(color);
}

void TextInput::UpdateInput(const TextInputInfo& info) {
  if (text_info_ == info)
    return;
  text_info_ = info;

  if (delegate_ && focused_)
    delegate_->UpdateInput(info);

  text_element_->SetText(info.text);
  text_element_->SetCursorPosition(info.selection_end);
  hint_element_->SetVisible(info.text.empty());
}

bool TextInput::OnBeginFrame(const base::TimeTicks& time,
                             const gfx::Vector3dF& look_at) {
  return SetCursorBlinkState(time);
}

void TextInput::OnSetSize(gfx::SizeF size) {
  hint_element_->SetSize(size.width(), size.height());
  text_element_->SetSize(size.width(), size.height());
}

void TextInput::OnSetName() {
  hint_element_->set_owner_name_for_test(name());
  text_element_->set_owner_name_for_test(name());
  cursor_element_->set_owner_name_for_test(name());
}

void TextInput::LayOutChildren() {
  // To avoid re-rendering a texture when the cursor blinks, the texture is a
  // separate element. Once the text has been laid out, we can position the
  // cursor appropriately relative to the text field.
  gfx::RectF bounds = text_element_->GetCursorBounds();
  cursor_element_->SetTranslate(bounds.x(), bounds.y(), 0);
  cursor_element_->SetSize(bounds.width(), bounds.height());
}

bool TextInput::SetCursorBlinkState(const base::TimeTicks& time) {
  base::TimeDelta delta = time - base::TimeTicks();
  bool visible =
      focused_ && delta.InMilliseconds() / kCursorBlinkHalfPeriodMs % 2;
  if (cursor_visible_ == visible)
    return false;
  cursor_visible_ = visible;
  cursor_element_->SetVisible(visible);
  return true;
}

}  // namespace vr
