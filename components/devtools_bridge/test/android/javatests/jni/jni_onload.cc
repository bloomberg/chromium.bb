// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "components/devtools_bridge/android/apiary_client_factory.h"
#include "components/devtools_bridge/android/session_dependency_factory_android.h"

namespace {

using namespace devtools_bridge::android;

bool RegisterJNI(JNIEnv* env) {
  return base::android::RegisterJni(env) &&
      SessionDependencyFactoryAndroid::RegisterNatives(env) &&
      ApiaryClientFactory::RegisterNatives(env);
}

bool Init() {
  return devtools_bridge::SessionDependencyFactory::InitializeSSL();
}

}  // namespace

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  std::vector<base::android::RegisterCallback> register_callbacks;
  register_callbacks.push_back(base::Bind(&RegisterJNI));
  std::vector<base::android::InitCallback> init_callbacks;
  init_callbacks.push_back(base::Bind(&Init));
  if (!base::android::OnJNIOnLoadRegisterJNI(vm, register_callbacks) ||
      !base::android::OnJNIOnLoadInit(init_callbacks)) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
