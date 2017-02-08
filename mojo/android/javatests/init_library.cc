// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "mojo/android/javatests/mojo_test_case.h"
#include "mojo/android/javatests/validation_test_util.h"
#include "mojo/android/system/core_impl.h"
#include "mojo/android/system/watcher_impl.h"
#include "mojo/edk/embedder/embedder.h"

namespace {

base::android::RegistrationMethod kMojoRegisteredMethods[] = {
    {"CoreImpl", mojo::android::RegisterCoreImpl},
    {"MojoTestCase", mojo::android::RegisterMojoTestCase},
    {"ValidationTestUtil", mojo::android::RegisterValidationTestUtil},
    {"WatcherImpl", mojo::android::RegisterWatcherImpl},
};

bool RegisterJNI(JNIEnv* env) {
  return base::android::RegisterJni(env) &&
         RegisterNativeMethods(env, kMojoRegisteredMethods,
                               arraysize(kMojoRegisteredMethods));
}

bool NativeInit() {
  if (!base::android::OnJNIOnLoadInit())
    return false;

  mojo::edk::Init();
  return true;
}

}  // namespace

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!base::android::OnJNIOnLoadRegisterJNI(env) || !RegisterJNI(env) ||
      !NativeInit()) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
