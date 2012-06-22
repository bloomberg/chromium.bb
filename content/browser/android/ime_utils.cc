// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/ime_utils.h"

#include <android/input.h>

#include "base/android/jni_android.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

namespace content {

jobject KeyEventFromNative(const NativeWebKeyboardEvent& event) {
  return event.os_event;
}

NativeWebKeyboardEvent NativeWebKeyboardEventFromKeyEvent(
    JNIEnv* env, jobject java_key_event, int action, int modifiers,
    long time_ms, int key_code, bool is_system_key, int unicode_char) {
  WebKit::WebInputEvent::Type type = WebKit::WebInputEvent::Undefined;
  if (action == AKEY_EVENT_ACTION_DOWN)
    type = WebKit::WebInputEvent::RawKeyDown;
  else if (action == AKEY_EVENT_ACTION_UP)
    type = WebKit::WebInputEvent::KeyUp;
  return NativeWebKeyboardEvent(java_key_event, type, modifiers,
      time_ms, key_code, unicode_char, is_system_key);
}

}  // namespace content
