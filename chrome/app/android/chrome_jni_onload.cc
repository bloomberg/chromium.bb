// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "chrome/app/android/chrome_android_initializer.h"
#include "content/public/app/content_jni_onload.h"

namespace {

bool RegisterJNI(JNIEnv* env) {
  return true;
}

bool Init() {
  // TODO(michaelbai): Move the JNI registration from RunChrome() to
  // RegisterJNI().
  return RunChrome();
}

}  // namespace


// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  std::vector<base::android::RegisterCallback> register_callbacks;
  register_callbacks.push_back(base::Bind(&RegisterJNI));
  std::vector<base::android::InitCallback> init_callbacks;
  init_callbacks.push_back(base::Bind(&Init));
  if (!content::android::OnJNIOnLoadRegisterJNI(vm, register_callbacks) ||
      !content::android::OnJNIOnLoadInit(init_callbacks)) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
