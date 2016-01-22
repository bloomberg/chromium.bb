// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_TOKEN_BINDING_MANAGER_BRIDGE_H_
#define ANDROID_WEBVIEW_NATIVE_TOKEN_BINDING_MANAGER_BRIDGE_H_

#include <jni.h>

namespace android_webview {

bool RegisterTokenBindingManager(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_TOKEN_BINDING_MANAGER_BRIDGE_H_
