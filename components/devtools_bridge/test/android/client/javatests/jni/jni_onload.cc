// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "chrome/app/android/chrome_android_initializer.h"
#include "chrome/app/android/chrome_main_delegate_android.h"
#include "components/devtools_bridge/android/session_dependency_factory_android.h"
#include "components/devtools_bridge/test/android/client/web_client_android.h"

using namespace devtools_bridge::android;

namespace {

class Delegate : public ChromeMainDelegateAndroid {
 public:
  bool RegisterApplicationNativeMethods(JNIEnv* env) override {
    return ChromeMainDelegateAndroid::RegisterApplicationNativeMethods(env) &&
           SessionDependencyFactoryAndroid::InitializeSSL() &&
           WebClientAndroid::RegisterNatives(env);
  }
};

}  // namespace

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  return RunChrome(vm, new Delegate());
}
