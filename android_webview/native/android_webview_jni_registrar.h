// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_ANDROID_WEBVIEW_JNI_REGISTRAR_H_
#define ANDROID_WEBVIEW_NATIVE_ANDROID_WEBVIEW_JNI_REGISTRAR_H_

#include <jni.h>

namespace android_webview {

// Register all JNI bindings necessary for chrome.
bool RegisterJni(JNIEnv* env);

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_ANDROID_WEBVIEW_JNI_REGISTRAR_H_
