// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/embedded_test_server/aw_test_jni_onload.h"
#include "base/android/jni_android.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!android_webview::test::OnJNIOnLoadRegisterJNI(env) ||
      !android_webview::test::OnJNIOnLoadInit()) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
