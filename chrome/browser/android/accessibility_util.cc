// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/android/accessibility_util.h"
#include "jni/AccessibilityUtil_jni.h"

namespace chrome {
namespace android {

bool AccessibilityUtil::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool AccessibilityUtil::IsAccessibilityEnabled() {
  return Java_AccessibilityUtil_isAccessibilityEnabled(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

AccessibilityUtil::AccessibilityUtil() { }

}  // namespace android
}  // namespace chrome
