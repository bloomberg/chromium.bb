// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/native_web_keyboard_event.h"

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : skip_in_browser(other.skip_in_browser) {
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  skip_in_browser = other.skip_in_browser;
  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
}
