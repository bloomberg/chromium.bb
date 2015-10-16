// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/ime_controller.h"

#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_text_input_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebWidget.h"

namespace html_viewer {

ImeController::ImeController(mus::Window* window, blink::WebWidget* widget)
    : window_(window), widget_(widget) {}

ImeController::~ImeController() {}

void ImeController::ResetInputMethod() {
  // TODO(penghuang): Reset IME.
}

void ImeController::DidHandleGestureEvent(const blink::WebGestureEvent& event,
                                          bool event_cancelled) {
  // Called when a gesture event is handled.
  if (event_cancelled)
    return;

  if (event.type == blink::WebInputEvent::GestureTap) {
    const bool show_ime = true;
    UpdateTextInputState(show_ime);
  } else if (event.type == blink::WebInputEvent::GestureLongPress) {
    // Only show IME if the textfield contains text.
    const bool show_ime = !widget_->textInputInfo().value.isEmpty();
    UpdateTextInputState(show_ime);
  }
}

void ImeController::DidUpdateTextOfFocusedElementByNonUserInput() {
  // Called when value of focused textfield gets dirty, e.g. value is
  // modified by script, not by user input.
  const bool show_ime = false;
  UpdateTextInputState(show_ime);
}

void ImeController::ShowImeIfNeeded() {
  // Request the browser to show the IME for current input type.
  const bool show_ime = true;
  UpdateTextInputState(show_ime);
}

void ImeController::UpdateTextInputState(bool show_ime) {
  blink::WebTextInputInfo new_info = widget_->textInputInfo();
  // Only show IME if the focused element is editable.
  show_ime = show_ime && new_info.type != blink::WebTextInputTypeNone;
  if (show_ime || text_input_info_ != new_info) {
    text_input_info_ = new_info;
    mojo::TextInputStatePtr state = mojo::TextInputState::New();
    state->type = mojo::ConvertTo<mojo::TextInputType>(new_info.type);
    state->flags = new_info.flags;
    state->text = mojo::String::From(new_info.value.utf8());
    state->selection_start = new_info.selectionStart;
    state->selection_end = new_info.selectionEnd;
    state->composition_start = new_info.compositionStart;
    state->composition_end = new_info.compositionEnd;
    if (show_ime)
      window_->SetImeVisibility(true, state.Pass());
    else
      window_->SetTextInputState(state.Pass());
  }
}

}  // namespace html_viewer
