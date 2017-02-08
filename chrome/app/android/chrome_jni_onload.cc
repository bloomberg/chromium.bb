// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_jni_onload.h"

#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "chrome/app/android/chrome_android_initializer.h"
#include "chrome/browser/android/chrome_jni_registrar.h"
#include "content/public/app/content_jni_onload.h"

namespace android {

bool OnJNIOnLoadRegisterJNI(JNIEnv* env) {
  if (!content::android::OnJNIOnLoadRegisterJNI(env))
    return false;

  if (base::android::GetLibraryProcessType(env) ==
      base::android::PROCESS_BROWSER) {
    return RegisterBrowserJNI(env);
  }
  return true;
}

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

  return RunChrome();
}

}  // namespace android
