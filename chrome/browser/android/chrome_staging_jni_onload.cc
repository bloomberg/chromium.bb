// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_staging_jni_onload.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "chrome/app/android/chrome_jni_onload.h"
#include "chrome/browser/android/staging_jni_registrar.h"

namespace chrome {
namespace android {

namespace {

bool RegisterJNI(JNIEnv* env) {
  if (base::android::GetLibraryProcessType(env) ==
      base::android::PROCESS_BROWSER) {
    return RegisterStagingJNI(env);
  }
  return true;
}

bool Init() {
  return true;
}

}  // namespace

bool OnJNIOnLoadRegisterJNI(JavaVM* vm) {
  return OnJNIOnLoadRegisterJNI(vm, base::Bind(&RegisterJNI));
}

bool OnJNIOnLoadInit() {
  return OnJNIOnLoadInit(base::Bind(&Init));
}

}  // namespace android
}  // namespace chrome
