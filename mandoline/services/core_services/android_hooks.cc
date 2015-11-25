// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/android/base_jni_onload.h"
#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "components/resource_provider/jni/Main_jni.h"
#include "net/android/net_jni_registrar.h"

namespace {
bool RegisterJNI(JNIEnv* env) {
  return net::android::RegisterJni(env);
}

bool Init() {
  return true;
}
}  // namespace


// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  std::vector<base::android::RegisterCallback> register_callbacks;
  register_callbacks.push_back(base::Bind(&RegisterJNI));
  register_callbacks.push_back(
      base::Bind(&resource_provider::RegisterNativesImpl));

  std::vector<base::android::InitCallback> init_callbacks;
  init_callbacks.push_back(base::Bind(&Init));

  if (!base::android::OnJNIOnLoadRegisterJNI(vm, register_callbacks) ||
      !base::android::OnJNIOnLoadInit(init_callbacks)) {
    return -1;
  }

  // There cannot be two AtExitManagers at the same time. Remove the one from
  // LibraryLoader as ApplicationRunner also uses one.
  base::android::LibraryLoaderExitHook();

  return JNI_VERSION_1_4;
}

extern "C" JNI_EXPORT void InitApplicationContext(
    const base::android::JavaRef<jobject>& context) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::InitApplicationContext(env, context);
  resource_provider::Java_Main_init(
      env, base::android::GetApplicationContext());
}
