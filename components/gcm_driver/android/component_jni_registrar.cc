// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/gcm_driver/gcm_driver_android.h"

namespace gcm {

namespace android {

static base::android::RegistrationMethod kGCMDriverRegisteredMethods[] = {
    {"GCMDriver", gcm::GCMDriverAndroid::RegisterJni},
};

bool RegisterGCMDriverJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env,
      kGCMDriverRegisteredMethods,
      arraysize(kGCMDriverRegisteredMethods));
}

}  // namespace android

}  // namespace gcm
