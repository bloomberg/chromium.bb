// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_webview_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/component/web_contents_delegate_android/component_jni_registrar.h"

namespace android_webview {

bool RegisterJni(JNIEnv* env) {
  return web_contents_delegate_android::RegisterJni(env);
}

} // namespace android_webview
