// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/native_web_keyboard_event.h"

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : skip_in_browser(false) {
  // TODO(saintlou: Aura stubs for now.
  NOTIMPLEMENTED();
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& event)
    : skip_in_browser(false) {
  // TODO(saintlou: Aura stubs for now.
  NOTIMPLEMENTED();
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  // TODO(saintlou: Aura stubs for now.
  NOTIMPLEMENTED();
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  // TODO(saintlou: Aura stubs for now.
  NOTIMPLEMENTED();
  return *this;
}

