// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/android/chrome_staging_jni_onload.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  if (!chrome::android::OnJNIOnLoadRegisterJNI(vm) ||
      !chrome::android::OnJNIOnLoadInit()) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
