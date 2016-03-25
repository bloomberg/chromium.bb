// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_method/input_method_engine.h"

#include "content/public/browser/render_frame_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace {

const char kErrorFollowCursorWindowExists[] =
    "A follow cursor IME window exists.";
const char kErrorNoInputFocus[] =
    "The follow cursor IME window cannot be created without an input focus.";
const char kErrorReachMaxWindowCount[] =
    "Cannot create more than 5 normal IME windows.";

const int kMaxNormalWindowCount = 5;

}  // namespace

namespace input_method {

InputMethodEngine::InputMethodEngine() : follow_cursor_window_(nullptr) {}

InputMethodEngine::~InputMethodEngine() {
  // Removes the listeners for OnWindowDestroyed.
  if (follow_cursor_window_)
    follow_cursor_window_->RemoveObserver(this);
  for (auto window : normal_windows_)
    window->RemoveObserver(this);

  CloseImeWindows();
}

bool InputMethodEngine::IsActive() const {
  return true;
}

std::string InputMethodEngine::GetExtensionId() const {
  return extension_id_;
}

int InputMethodEngine::CreateImeWindow(
    const extensions::Extension* extension,
    content::RenderFrameHost* render_frame_host,
    const std::string& url,
    ui::ImeWindow::Mode mode,
    const gfx::Rect& bounds,
    std::string* error) {
  if (mode == ui::ImeWindow::FOLLOW_CURSOR) {
    if (follow_cursor_window_) {
      *error = kErrorFollowCursorWindowExists;
      return 0;
    }
    if (current_input_type_ == ui::TEXT_INPUT_TYPE_NONE) {
      *error = kErrorNoInputFocus;
      return 0;
    }
  }

  if (mode == ui::ImeWindow::NORMAL &&
      normal_windows_.size() >= kMaxNormalWindowCount) {
    *error = kErrorReachMaxWindowCount;
    return 0;
  }

  // ui::ImeWindow manages its own lifetime.
  ui::ImeWindow* ime_window = new ui::ImeWindow(
      profile_, extension, render_frame_host, url, mode, bounds);
  ime_window->AddObserver(this);
  ime_window->Show();
  if (mode == ui::ImeWindow::FOLLOW_CURSOR) {
    follow_cursor_window_ = ime_window;
    ime_window->FollowCursor(current_cursor_bounds_);
  } else {
    normal_windows_.push_back(ime_window);
  }

  return ime_window->GetFrameId();
}

void InputMethodEngine::ShowImeWindow(int window_id) {
  ui::ImeWindow* ime_window = FindWindowById(window_id);
  if (ime_window)
    ime_window->Show();
}

void InputMethodEngine::HideImeWindow(int window_id) {
  ui::ImeWindow* ime_window = FindWindowById(window_id);
  if (ime_window)
    ime_window->Hide();
}

void InputMethodEngine::CloseImeWindows() {
  if (follow_cursor_window_)
    follow_cursor_window_->Close();
  for (auto window : normal_windows_)
    window->Close();
  normal_windows_.clear();
}

void InputMethodEngine::FocusOut() {
  InputMethodEngineBase::FocusOut();
  if (follow_cursor_window_)
    follow_cursor_window_->Hide();
}

void InputMethodEngine::SetCompositionBounds(
    const std::vector<gfx::Rect>& bounds) {
  InputMethodEngineBase::SetCompositionBounds(bounds);
  if (!bounds.empty()) {
    current_cursor_bounds_ = bounds[0];
    if (follow_cursor_window_)
      follow_cursor_window_->FollowCursor(current_cursor_bounds_);
  }
}

void InputMethodEngine::UpdateComposition(
    const ui::CompositionText& composition_text,
    uint32_t cursor_pos,
    bool is_visible) {
  composition_.CopyFrom(composition_text);

  // Use a black thin underline by default.
  if (composition_.underlines.empty()) {
    composition_.underlines.push_back(
        ui::CompositionUnderline(0, composition_.text.length(), SK_ColorBLACK,
                                 false /* thick */, SK_ColorTRANSPARENT));
  }

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  // If the IME extension is handling key event, hold the composition text
  // until the key event is handled.
  if (input_context && !handling_key_event_) {
    input_context->UpdateCompositionText(composition_, cursor_pos, is_visible);
    composition_.Clear();
  }
}

void InputMethodEngine::CommitTextToInputContext(int context_id,
                                                 const std::string& text) {
  // Append the text to the buffer, as it allows committing text multiple times
  // when processing a key event.
  text_ += text;

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  // If the IME extension is handling key event, hold the text until the key
  // event is handled.
  if (input_context && !handling_key_event_) {
    input_context->CommitText(text_);
    text_ = "";
  }
}

void InputMethodEngine::OnWindowDestroyed(ui::ImeWindow* ime_window) {
  if (ime_window == follow_cursor_window_) {
    follow_cursor_window_ = nullptr;
  } else {
    auto it = std::find(
        normal_windows_.begin(), normal_windows_.end(), ime_window);
    if (it != normal_windows_.end())
      normal_windows_.erase(it);
  }
}

ui::ImeWindow* InputMethodEngine::FindWindowById(int window_id) const {
  if (follow_cursor_window_ &&
      follow_cursor_window_->GetFrameId() == window_id) {
    return follow_cursor_window_;
  }
  for (auto ime_window : normal_windows_) {
    if (ime_window->GetFrameId() == window_id)
      return ime_window;
  }
  return nullptr;
}

bool InputMethodEngine::SendKeyEvent(ui::KeyEvent* event,
                                     const std::string& code) {
  DCHECK(event);
  if (event->key_code() == ui::VKEY_UNKNOWN)
    event->set_key_code(ui::DomCodeToUsLayoutKeyboardCode(event->code()));

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return false;
  input_context->SendKeyEvent(event);

  return true;
}

}  // namespace input_method
