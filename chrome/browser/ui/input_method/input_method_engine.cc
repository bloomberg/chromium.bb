// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_method/input_method_engine.h"

#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"

namespace input_method {

InputMethodEngine::InputMethodEngine() {}

InputMethodEngine::~InputMethodEngine() {}

bool InputMethodEngine::SendKeyEvents(
    int context_id,
    const std::vector<KeyboardEvent>& events) {
  // TODO(azurewei) Implement SendKeyEvents funciton
  return false;
}

bool InputMethodEngine::IsActive() const {
  return true;
}

std::string InputMethodEngine::GetExtensionId() const {
  return extension_id_;
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
    input_context->UpdateCompositionText(composition_text, cursor_pos,
                                         is_visible);
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

}  // namespace input_method
