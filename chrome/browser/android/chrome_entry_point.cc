// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "chrome/app/android/chrome_jni_onload.h"

namespace {

bool RegisterJNI(JNIEnv* env) {
  return true;
}

bool Init() {
  return true;
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  if (!chrome::android::OnJNIOnLoadRegisterJNI(vm, base::Bind(&RegisterJNI)) ||
      !chrome::android::OnJNIOnLoadInit(base::Bind(&Init))) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
