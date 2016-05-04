// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/webview_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
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
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  bool ret;
  int library_process_type = base::android::GetLibraryProcessType(env);
  if (library_process_type == base::android::PROCESS_WEBVIEW ||
      library_process_type == base::android::PROCESS_WEBVIEW_CHILD) {
    base::android::DisableManualJniRegistration();
    ret = android_webview::OnJNIOnLoadInit();
  } else {
    ret = chrome::android::OnJNIOnLoadRegisterJNI(vm,
                                                  base::Bind(&RegisterJNI)) &&
      chrome::android::OnJNIOnLoadInit(base::Bind(&Init));
  }
  return ret ? JNI_VERSION_1_4 : -1;
}
