// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vibration/android/vibration_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "device/vibration/vibration_manager_impl_android.h"

namespace device {
namespace android {
namespace {

const base::android::RegistrationMethod kRegisteredMethods[] = {
    { "VibrationProvider", device::VibrationManagerImplAndroid::Register },
};

}  // namespace

bool RegisterVibrationJni(JNIEnv* env) {
  return RegisterNativeMethods(
      env, kRegisteredMethods, arraysize(kRegisteredMethods));
}

}  // namespace android
}  // namespace device
