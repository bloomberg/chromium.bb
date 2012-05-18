// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "base/android/jni_android.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/android/WebInputEventFactory.h"
#include "ui/gfx/native_widget_types.h"

using WebKit::WebInputEventFactory;

namespace {

jobject NewGlobalRefForKeyEvent(jobject key_event) {
  if (key_event == NULL) return NULL;
  return base::android::AttachCurrentThread()->NewGlobalRef(key_event);
}

void DeleteGlobalRefForKeyEvent(jobject key_event) {
  if (key_event != NULL)
    base::android::AttachCurrentThread()->DeleteGlobalRef(key_event);
}

}

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    WebKit::WebInputEvent::Type type,
    int modifiers, double time_secs, int keycode, int unicode_character,
    bool is_system_key)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(
        type, modifiers, time_secs, keycode, unicode_character,
        is_system_key)) {
  os_event = NULL;
  skip_in_browser = false;
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    jobject android_key_event, WebKit::WebInputEvent::Type type,
    int modifiers, double time_secs, int keycode, int unicode_character,
    bool is_system_key)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(
        type, modifiers, time_secs, keycode, unicode_character,
        is_system_key)) {
  os_event = NewGlobalRefForKeyEvent(android_key_event);
  skip_in_browser = false;
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(NewGlobalRefForKeyEvent(other.os_event)),
      skip_in_browser(other.skip_in_browser) {
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  os_event = NewGlobalRefForKeyEvent(other.os_event);
  skip_in_browser = other.skip_in_browser;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  DeleteGlobalRefForKeyEvent(os_event);
}

}  // namespace content
