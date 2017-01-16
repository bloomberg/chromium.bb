// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/input_method_bridge_chromeos.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"

InputMethodBridge::InputMethodBridge(
    std::unique_ptr<RemoteTextInputClient> client)
    : client_(std::move(client)),
      input_method_chromeos_(
          base::MakeUnique<ui::InputMethodChromeOS>(nullptr)) {
  input_method_chromeos_->SetFocusedTextInputClient(client_.get());
}

InputMethodBridge::~InputMethodBridge() {}

void InputMethodBridge::OnTextInputTypeChanged(
    ui::TextInputType text_input_type) {
  client_->SetTextInputType(text_input_type);
  input_method_chromeos_->OnTextInputTypeChanged(client_.get());
}

void InputMethodBridge::OnCaretBoundsChanged(const gfx::Rect& caret_bounds) {
  client_->SetCaretBounds(caret_bounds);
  input_method_chromeos_->OnCaretBoundsChanged(client_.get());
}

void InputMethodBridge::ProcessKeyEvent(
    std::unique_ptr<ui::Event> event,
    const ProcessKeyEventCallback& callback) {
  DCHECK(event->IsKeyEvent());
  ui::KeyEvent* key_event = event->AsKeyEvent();
  if (!key_event->is_char()) {
    input_method_chromeos_->DispatchKeyEvent(
        key_event, base::MakeUnique<base::Callback<void(bool)>>(callback));
  } else {
    // On Linux (include ChromeOS), the mus emulates the WM_CHAR generation
    // behaviour of Windows. But for ChromeOS, we don't expect those char
    // events, so we filter them out.
    const bool handled = true;
    callback.Run(handled);
  }
}

void InputMethodBridge::CancelComposition() {
  input_method_chromeos_->CancelComposition(client_.get());
}
