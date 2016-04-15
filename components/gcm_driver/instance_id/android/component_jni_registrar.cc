// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/gcm_driver/instance_id/instance_id_android.h"

namespace instance_id {
namespace android {

static base::android::RegistrationMethod kInstanceIDRegisteredMethods[] = {
    {"InstanceID", instance_id::InstanceIDAndroid::RegisterJni},
};

bool RegisterInstanceIDJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kInstanceIDRegisteredMethods,
      arraysize(kInstanceIDRegisteredMethods));
}

}  // namespace android
}  // namespace instance_id
