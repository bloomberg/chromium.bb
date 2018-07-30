// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_factory.h"

#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/ui.h"

namespace vr {

std::unique_ptr<UiInterface> UiFactory::Create(
    UiBrowserInterface* browser,
    PlatformInputHandler* content_input_forwarder,
    KeyboardDelegate* keyboard_delegate,
    TextInputDelegate* text_input_delegate,
    AudioDelegate* audio_delegate,
    const UiInitialState& ui_initial_state) {
  return std::make_unique<Ui>(browser, content_input_forwarder,
                              keyboard_delegate, text_input_delegate,
                              audio_delegate, ui_initial_state);
}

}  // namespace vr
