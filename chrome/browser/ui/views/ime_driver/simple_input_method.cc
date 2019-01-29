// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/simple_input_method.h"

#include <utility>

SimpleInputMethod::SimpleInputMethod() {}

SimpleInputMethod::~SimpleInputMethod() {}

void SimpleInputMethod::OnTextInputStateChanged(
    ws::mojom::TextInputStatePtr text_input_state) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SimpleInputMethod::OnCaretBoundsChanged(const gfx::Rect& caret_bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SimpleInputMethod::OnTextInputClientDataChanged(
    ws::mojom::TextInputClientDataPtr data) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SimpleInputMethod::ProcessKeyEvent(std::unique_ptr<ui::Event> key_event,
                                        ProcessKeyEventCallback callback) {
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(false);
}

void SimpleInputMethod::CancelComposition() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SimpleInputMethod::ShowVirtualKeyboardIfEnabled() {
  NOTIMPLEMENTED_LOG_ONCE();
}
