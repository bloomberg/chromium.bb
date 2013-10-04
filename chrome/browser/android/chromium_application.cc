// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chromium_application.h"

#include "base/android/jni_android.h"
#include "jni/ChromiumApplication_jni.h"

namespace chrome {
namespace android {

// static
bool ChromiumApplication::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ChromiumApplication::OpenProtectedContentSettings() {
  Java_ChromiumApplication_openProtectedContentSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

}  // namespace android
}  // namespace chrome
