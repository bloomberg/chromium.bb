// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "chrome/app/android/chrome_jni_onload.h"

namespace {

bool NativeInit() {
  return android::OnJNIOnLoadInit();
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // By default, all JNI methods are registered. However, since render processes
  // don't need very much Java code, we enable selective JNI registration on the
  // Java side and only register a subset of JNI methods.
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (base::android::IsSelectiveJniRegistrationEnabled(env)) {
    base::android::SetJniRegistrationType(
        base::android::SELECTIVE_JNI_REGISTRATION);
  }
  if (!android::OnJNIOnLoadRegisterJNI(env)) {
    return -1;
  }
  base::android::SetNativeInitializationHook(NativeInit);
  return JNI_VERSION_1_4;
}
