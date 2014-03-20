// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ANDROID_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_ANDROID_ANDROID_ACCESSIBILITY_UTIL_H_

#include "base/android/jni_android.h"

namespace chrome {
namespace android {

class AccessibilityUtil {
 public:
  static bool Register(JNIEnv* env);

  static bool IsAccessibilityEnabled();

 private:
  AccessibilityUtil();
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_ANDROID_ACCESSIBILITY_UTIL_H_
