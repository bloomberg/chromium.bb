// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/android/jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/safe_browsing_db/android/safe_browsing_api_handler_bridge.h"

namespace safe_browsing {
namespace android {

static base::android::RegistrationMethod kSafeBrowsingRegisteredMethods[] = {
    {"SafeBrowsingApiBridge", safe_browsing::RegisterSafeBrowsingApiBridge},
};

bool RegisterBrowserJNI(JNIEnv* env) {
  return RegisterNativeMethods(env, kSafeBrowsingRegisteredMethods,
                               arraysize(kSafeBrowsingRegisteredMethods));
}

}  // namespace android
}  // namespace safe_browsing
