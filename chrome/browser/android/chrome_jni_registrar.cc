// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/android/process_utils.h"

namespace chrome {
namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
  { "IntentHelper", RegisterIntentHelper },
  { "ProcessUtils", RegisterProcessUtils },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kChromeRegisteredMethods,
                               arraysize(kChromeRegisteredMethods));
}

} // namespace android
} // namespace chrome
