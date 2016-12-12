// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reduce number of log messages by logging each NOTIMPLEMENTED() only once.
// This has to be before any other includes, else default is picked up.
// See base/logging.h for details on this.
#define NOTIMPLEMENTED_POLICY 5

#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"

#include "base/strings/utf_string_conversions.h"

RemoteTextInputClient::RemoteTextInputClient(
    ui::mojom::TextInputClientPtr remote_client)
    : remote_client_(std::move(remote_client)) {}

RemoteTextInputClient::~RemoteTextInputClient() {}

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
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return ui::TEXT_INPUT_TYPE_TEXT;
}

ui::TextInputMode RemoteTextInputClient::GetTextInputMode() const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection RemoteTextInputClient::GetTextDirection() const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return base::i18n::UNKNOWN_DIRECTION;
}

int RemoteTextInputClient::GetTextInputFlags() const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return 0;
}

bool RemoteTextInputClient::CanComposeInline() const {
  // If we return false here, ui::InputMethodChromeOS will try to create a
  // composition window. But here we are at IMEDriver, and composition
  // window shouldn't be created by IMEDriver.
  return true;
}

gfx::Rect RemoteTextInputClient::GetCaretBounds() const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED();
  return gfx::Rect();
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
