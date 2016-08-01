// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/android/gamepad_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "device/gamepad/gamepad_platform_data_fetcher_android.h"

namespace device {
namespace android {
namespace {

const base::android::RegistrationMethod kRegisteredMethods[] = {
    {"GamepadPlatformDataFetcherAndroid",
     device::GamepadPlatformDataFetcherAndroid::
         RegisterGamepadPlatformDataFetcherAndroid},
};

}  // namespace

bool RegisterGamepadJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kRegisteredMethods,
                               arraysize(kRegisteredMethods));
}

}  // namespace android
}  // namespace device
