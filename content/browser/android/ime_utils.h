// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IME_UTILS_H_
#define CONTENT_BROWSER_ANDROID_IME_UTILS_H_
#pragma once

#include <jni.h>

class AndroidKeyEvent;

namespace content {

struct NativeWebKeyboardEvent;

// Returns a java KeyEvent from a NativeWebKeyboardEvent, NULL if it fails.
jobject KeyEventFromNative(const NativeWebKeyboardEvent& event);

// Maps a java KeyEvent into a NativeWebKeyboardEvent.
// |java_key_event| is used to maintain a globalref for KeyEvent.
// |action| will help determine the WebInputEvent type.
// type, |modifiers|, |time_ms|, |key_code|, |unicode_char| is used to create
// WebKeyboardEvent. |key_code| is also needed ad need to treat the enter key
// as a key press of character \r.
NativeWebKeyboardEvent NativeWebKeyboardEventFromKeyEvent(
    JNIEnv* env, jobject java_key_event, int action, int modifiers,
    long time_ms, int key_code, bool is_system_key, int unicode_char);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IME_UTILS_H_
