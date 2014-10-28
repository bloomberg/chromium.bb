// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/android/battery_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "device/battery/battery_status_manager_android.h"

namespace device {
namespace android {
namespace {

base::android::RegistrationMethod kRegisteredMethods[] = {
    { "BatteryStatusManager", BatteryStatusManagerAndroid::Register },
};

}  // namespace

bool RegisterBatteryJni(JNIEnv* env) {
  return RegisterNativeMethods(
      env, kRegisteredMethods, arraysize(kRegisteredMethods));
}

}  // namespace android
}  // namespace device
