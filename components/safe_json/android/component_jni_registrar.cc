// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/basictypes.h"
#include "components/safe_json/json_sanitizer.h"

namespace safe_json {
namespace android {

static base::android::RegistrationMethod kSafeJsonRegisteredMethods[] = {
    {"JsonSanitizer", JsonSanitizer::Register},
};

bool RegisterSafeJsonJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kSafeJsonRegisteredMethods, arraysize(kSafeJsonRegisteredMethods));
}

}  // namespace android
}  // namespace safe_json
