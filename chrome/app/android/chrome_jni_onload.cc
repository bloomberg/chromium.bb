// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_jni_onload.h"

#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "chrome/app/android/chrome_android_initializer.h"
#include "chrome/browser/android/chrome_jni_registrar.h"
#include "content/public/app/content_jni_onload.h"

namespace chrome {
namespace android {

namespace {

bool RegisterJNI(JNIEnv* env) {
  if (base::android::GetLibraryProcessType(env) ==
      base::android::PROCESS_BROWSER) {
    return RegisterBrowserJNI(env);
  }
  return true;
}

bool Init() {
  return RunChrome();
}

}  // namespace

bool OnJNIOnLoadRegisterJNI(
    JavaVM* vm,
    base::android::RegisterCallback callback) {
  std::vector<base::android::RegisterCallback> register_callbacks;
  register_callbacks.push_back(callback);
  register_callbacks.push_back(base::Bind(&RegisterJNI));
  return content::android::OnJNIOnLoadRegisterJNI(vm, register_callbacks);
}

bool OnJNIOnLoadInit(base::android::InitCallback callback) {
  std::vector<base::android::InitCallback> init_callbacks;
  init_callbacks.push_back(callback);
  init_callbacks.push_back(base::Bind(&Init));
  return content::android::OnJNIOnLoadInit(init_callbacks);
}

}  // namespace android
}  // namespace chrome
