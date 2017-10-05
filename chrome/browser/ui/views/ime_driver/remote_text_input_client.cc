// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reduce number of log messages by logging each NOTIMPLEMENTED() only once.
// This has to be before any other includes, else default is picked up.
// See base/logging.h for details on this.
#define NOTIMPLEMENTED_POLICY 5

#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"

#include "base/strings/utf_string_conversions.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/ime_bridge.h"
#endif

RemoteTextInputClient::RemoteTextInputClient(
    ui::mojom::TextInputClientPtr remote_client,
    ui::TextInputType text_input_type,
    ui::TextInputMode text_input_mode,
    base::i18n::TextDirection text_direction,
    int text_input_flags,
    gfx::Rect caret_bounds)
    : remote_client_(std::move(remote_client)),
      text_input_type_(text_input_type),
      text_input_mode_(text_input_mode),
      text_direction_(text_direction),
      text_input_flags_(text_input_flags),
      caret_bounds_(caret_bounds) {
#if defined(OS_CHROMEOS)
  ui::IMEBridge::Get()->SetCandidateWindowHandler(this);
#endif
}

RemoteTextInputClient::~RemoteTextInputClient() {}

void RemoteTextInputClient::SetTextInputType(
    ui::TextInputType text_input_type) {
  text_input_type_ = text_input_type;
}

void RemoteTextInputClient::SetCaretBounds(const gfx::Rect& caret_bounds) {
  caret_bounds_ = caret_bounds;
}

void RemoteTextInputClient::SetCompositionText(
    const ui::CompositionText& composition) {
  remote_client_->SetCompositionText(composition);
}

void RemoteTextInputClient::ConfirmCompositionText() {
  remote_client_->ConfirmCompositionText();
}

void RemoteTextInputClient::ClearCompositionText() {
  remote_client_->ClearCompositionText();
}

void RemoteTextInputClient::InsertText(const base::string16& text) {
  remote_client_->InsertText(base::UTF16ToUTF8(text));
}

void RemoteTextInputClient::InsertChar(const ui::KeyEvent& event) {
  remote_client_->InsertChar(ui::Event::Clone(event));
}

ui::TextInputType RemoteTextInputClient::GetTextInputType() const {
  return text_input_type_;
}

ui::TextInputMode RemoteTextInputClient::GetTextInputMode() const {
  return text_input_mode_;
}

base::i18n::TextDirection RemoteTextInputClient::GetTextDirection() const {
  return text_direction_;
}

int RemoteTextInputClient::GetTextInputFlags() const {
  return text_input_flags_;
}

bool RemoteTextInputClient::CanComposeInline() const {
  // If we return false here, ui::InputMethodChromeOS will try to create a
  // composition window. But here we are at IMEDriver, and composition
  // window shouldn't be created by IMEDriver.
  return true;
}

gfx::Rect RemoteTextInputClient::GetCaretBounds() const {
  return caret_bounds_;
}

bool RemoteTextInputClient::GetCompositionCharacterBounds(
    uint32_t index,
    gfx::Rect* rect) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTextInputClient::HasCompositionText() const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTextInputClient::GetTextRange(gfx::Range* range) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTextInputClient::GetCompositionTextRange(gfx::Range* range) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTextInputClient::GetSelectionRange(gfx::Range* range) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTextInputClient::SetSelectionRange(const gfx::Range& range) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTextInputClient::DeleteRange(const gfx::Range& range) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTextInputClient::GetTextFromRange(const gfx::Range& range,
                                             base::string16* text) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

void RemoteTextInputClient::OnInputMethodChanged() {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
}

bool RemoteTextInputClient::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

void RemoteTextInputClient::ExtendSelectionAndDelete(size_t before,
                                                     size_t after) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
}

void RemoteTextInputClient::EnsureCaretNotInRect(const gfx::Rect& rect) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
}

bool RemoteTextInputClient::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return false;
}

void RemoteTextInputClient::SetTextEditCommandForNextKeyEvent(
    ui::TextEditCommand command) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
}

ui::EventDispatchDetails RemoteTextInputClient::DispatchKeyEventPostIME(
    ui::KeyEvent* event) {
  remote_client_->DispatchKeyEventPostIME(ui::Event::Clone(*event),
                                          base::OnceCallback<void(bool)>());
  return ui::EventDispatchDetails();
}

void RemoteTextInputClient::UpdateLookupTable(
    const ui::CandidateWindow& candidate_window,
    bool visible) {}

void RemoteTextInputClient::UpdatePreeditText(const base::string16& text,
                                              uint32_t cursor_pos,
                                              bool visible) {}

void RemoteTextInputClient::SetCursorBounds(const gfx::Rect& cursor_bounds,
                                            const gfx::Rect& composition_head) {
}

void RemoteTextInputClient::OnCandidateWindowVisibilityChanged(bool visible) {
#if defined(OS_CHROMEOS)
  remote_client_->SetCandidateWindowVisible(visible);
#endif
}
