// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/input_connection_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/ime/ime_bridge.h"

namespace arc {

namespace {

// Timeout threshold after the IME operation is sent to TextInputClient.
// If no text input state observer methods in below ArcProxyInputMethodObserver
// is called during this time period, the current text input state is sent to
// Android.
// TODO(yhanada): Implement a way to observe an IME operation completion and
// send the current text input state right after the IME operation completion.
constexpr base::TimeDelta kStateUpdateTimeout = base::TimeDelta::FromSeconds(1);

}  // namespace

InputConnectionImpl::InputConnectionImpl(
    chromeos::InputMethodEngine* ime_engine,
    ArcInputMethodManagerBridge* imm_bridge,
    int input_context_id)
    : ime_engine_(ime_engine),
      imm_bridge_(imm_bridge),
      input_context_id_(input_context_id),
      binding_(this),
      composing_text_(),
      state_update_timer_() {}

InputConnectionImpl::~InputConnectionImpl() = default;

void InputConnectionImpl::Bind(mojom::InputConnectionPtr* interface_ptr) {
  binding_.Bind(mojo::MakeRequest(interface_ptr));
}

void InputConnectionImpl::UpdateTextInputState(
    bool is_input_state_update_requested) {
  if (state_update_timer_.IsRunning()) {
    // There is a pending request.
    is_input_state_update_requested = true;
  }
  state_update_timer_.Stop();
  imm_bridge_->SendUpdateTextInputState(
      GetTextInputState(is_input_state_update_requested));
}

mojom::TextInputStatePtr InputConnectionImpl::GetTextInputState(
    bool is_input_state_update_requested) const {
  ui::IMEBridge* bridge = ui::IMEBridge::Get();
  DCHECK(bridge);
  ui::IMEInputContextHandlerInterface* handler =
      bridge->GetInputContextHandler();
  DCHECK(handler);
  ui::TextInputClient* client = handler->GetInputMethod()->GetTextInputClient();
  DCHECK(client);

  gfx::Range text_range, selection_range;
  base::string16 text;
  client->GetTextRange(&text_range);
  client->GetSelectionRange(&selection_range);
  client->GetTextFromRange(text_range, &text);

  return mojom::TextInputStatePtr(
      base::in_place, selection_range.start(), text, text_range,
      selection_range, client->GetTextInputType(), client->ShouldDoLearning(),
      client->GetTextInputFlags(), is_input_state_update_requested);
}

void InputConnectionImpl::CommitText(const base::string16& text,
                                     int new_cursor_pos) {
  StartStateUpdateTimer();

  std::string error;
  // Clear the current composing text at first.
  if (!ime_engine_->ClearComposition(input_context_id_, &error))
    LOG(ERROR) << "ClearComposition failed: error=\"" << error << "\"";
  if (!ime_engine_->CommitText(input_context_id_,
                               base::UTF16ToUTF8(text).c_str(), &error))
    LOG(ERROR) << "CommitText failed: error=\"" << error << "\"";
}

void InputConnectionImpl::DeleteSurroundingText(int before, int after) {
  StartStateUpdateTimer();

  std::string error;
  // DeleteSurroundingText takes a start position relative to the current cursor
  // position and a length of the text is going to be deleted.
  // |before| is a number of characters is going to be deleted before the cursor
  // and |after| is a number of characters is going to be deleted after the
  // cursor.
  if (!ime_engine_->DeleteSurroundingText(input_context_id_, -before,
                                          before + after, &error)) {
    LOG(ERROR) << "DeleteSurroundingText failed: before = " << before
               << ", after = " << after << ", error = \"" << error << "\"";
  }
}

void InputConnectionImpl::FinishComposingText() {
  StartStateUpdateTimer();

  std::string error;
  if (!ime_engine_->CommitText(input_context_id_,
                               base::UTF16ToUTF8(composing_text_).c_str(),
                               &error)) {
    LOG(ERROR) << "FinishComposingText: CommitText() failed, error=\"" << error
               << "\"";
  }
  composing_text_.clear();
}

void InputConnectionImpl::SetComposingText(const base::string16& text,
                                           int new_cursor_pos) {
  // If new_cursor_pos > 0, it's relative to (the end of the text - 1).
  if (new_cursor_pos > 0)
    new_cursor_pos += text.length() - 1;

  StartStateUpdateTimer();

  std::string error;
  if (!ime_engine_->SetComposition(
          input_context_id_, base::UTF16ToUTF8(text).c_str(), 0, 0,
          new_cursor_pos,
          std::vector<input_method::InputMethodEngineBase::SegmentInfo>(),
          &error)) {
    LOG(ERROR) << "SetComposingText failed: pos=" << new_cursor_pos
               << ", error=\"" << error << "\"";
    return;
  }
  composing_text_ = text;
}

void InputConnectionImpl::RequestTextInputState(
    mojom::InputConnection::RequestTextInputStateCallback callback) {
  std::move(callback).Run(GetTextInputState(false));
}

void InputConnectionImpl::StartStateUpdateTimer() {
  // It's safe to use Unretained() here because the timer is automatically
  // canceled when it go out of scope.
  state_update_timer_.Start(
      FROM_HERE, kStateUpdateTimeout,
      base::BindOnce(&InputConnectionImpl::UpdateTextInputState,
                     base::Unretained(this),
                     true /* is_input_state_update_requested */));
}

}  // namespace arc
