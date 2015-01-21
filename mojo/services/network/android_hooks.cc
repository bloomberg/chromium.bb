// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "net/android/net_jni_registrar.h"

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!base::android::RegisterJni(env))
    return -1;

  if (!net::android::RegisterJni(env))
    return -1;

  return JNI_VERSION_1_4;
}

extern "C" JNI_EXPORT void InitApplicationContext(
    const base::android::JavaRef<jobject>& context) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::InitApplicationContext(env, context);
}
