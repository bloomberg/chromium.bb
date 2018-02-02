// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text_input.h"

#include <memory>

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/geometry/rect.h"

namespace {
constexpr int kCursorBlinkHalfPeriodMs = 600;
}

namespace vr {

TextInput::TextInput(float font_height_meters,
                     OnInputEditedCallback input_edit_callback)
    : input_edit_callback_(input_edit_callback) {
  auto text = std::make_unique<Text>(font_height_meters);
  text->SetType(kTypeTextInputHint);
  text->SetDrawPhase(kPhaseForeground);
  text->set_hit_testable(false);
  text->set_focusable(false);
  text->set_x_anchoring(LEFT);
  text->set_x_centering(LEFT);
  text->SetSize(1, 1);
  text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  text->SetAlignment(UiTexture::kTextAlignmentLeft);
  hint_element_ = text.get();
  this->AddChild(std::move(text));

  text = std::make_unique<Text>(font_height_meters);
  text->SetType(kTypeTextInputText);
  text->SetDrawPhase(kPhaseForeground);
  text->set_hit_testable(true);
  text->set_focusable(false);
  text->set_x_anchoring(LEFT);
  text->set_x_centering(LEFT);
  text->set_bubble_events(true);
  text->SetSize(1, 1);
  text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  text->SetAlignment(UiTexture::kTextAlignmentLeft);
  text->SetCursorEnabled(true);
  text_element_ = text.get();
  this->AddChild(std::move(text));

  auto cursor = std::make_unique<Rect>();
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
}

TextInput::~TextInput() {}

void TextInput::SetTextInputDelegate(TextInputDelegate* text_input_delegate) {
  delegate_ = text_input_delegate;
}

void TextInput::OnButtonDown(const gfx::PointF& position) {
  // Reposition the cursor based on click position.
  int cursor_position = text_element_->GetCursorPositionFromPoint(position);
  TextInputInfo info(text_info_);
  info.selection_start = cursor_position;
  info.selection_end = cursor_position;
  if (text_info_ != info) {
    UpdateInput(info);
    ResetCursorBlinkCycle();
  }
}

void TextInput::OnButtonUp(const gfx::PointF& position) {
  RequestFocus();
}

void TextInput::OnFocusChanged(bool focused) {
  focused_ = focused;

  // Update the keyboard with the current text.
  if (delegate_ && focused)
    delegate_->UpdateInput(text_info_);

  if (event_handlers_.focus_change)
    event_handlers_.focus_change.Run(focused);
}

void TextInput::RequestFocus() {
  if (!delegate_ || focused_)
    return;

  delegate_->RequestFocus(id());
}

void TextInput::RequestUnfocus() {
  if (!delegate_ || !focused_)
    return;

  delegate_->RequestUnfocus(id());
}

void TextInput::SetHintText(const base::string16& text) {
  hint_element_->SetText(text);
}

void TextInput::OnInputEdited(const TextInputInfo& info) {
  if (input_edit_callback_)
    input_edit_callback_.Run(info);
}

void TextInput::OnInputCommitted(const TextInputInfo& info) {
  if (input_commit_callback_)
    input_commit_callback_.Run(info);
}

void TextInput::SetTextColor(SkColor color) {
  text_element_->SetColor(color);
}

void TextInput::SetHintColor(SkColor color) {
  hint_element_->SetColor(color);
}

void TextInput::SetSelectionColors(const TextSelectionColors& colors) {
  cursor_element_->SetColor(colors.cursor);
  text_element_->SetSelectionColors(colors);
}

void TextInput::UpdateInput(const TextInputInfo& info) {
  if (text_info_ == info)
    return;

  OnUpdateInput(info, text_info_);

  text_info_ = info;

  if (delegate_ && focused_)
    delegate_->UpdateInput(info);

  text_element_->SetText(info.text);
  text_element_->SetSelectionIndices(info.selection_start, info.selection_end);
  hint_element_->SetVisible(info.text.empty());
}

bool TextInput::OnBeginFrame(const base::TimeTicks& time,
                             const gfx::Transform& head_pose) {
  return SetCursorBlinkState(time);
}

void TextInput::OnSetSize(const gfx::SizeF& size) {
  hint_element_->SetSize(size.width(), size.height());
  text_element_->SetSize(size.width(), size.height());
}

void TextInput::OnSetName() {
  hint_element_->set_owner_name_for_test(name());
  text_element_->set_owner_name_for_test(name());
  cursor_element_->set_owner_name_for_test(name());
}

TextInputInfo TextInput::GetTextInputInfoForTest() const {
  return text_info_;
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
  base::TimeDelta delta = time - cursor_blink_start_ticks_;
  bool visible = focused_ && text_info_.SelectionSize() == 0 &&
                 (delta.InMilliseconds() / kCursorBlinkHalfPeriodMs + 1) % 2;
  if (cursor_visible_ == visible)
    return false;
  cursor_visible_ = visible;
  cursor_element_->SetVisible(visible);
  return true;
}

void TextInput::ResetCursorBlinkCycle() {
  cursor_blink_start_ticks_ = base::TimeTicks::Now();
}

void TextInput::OnUpdateInput(const TextInputInfo& info,
                              const TextInputInfo& previous_info) {}

}  // namespace vr
