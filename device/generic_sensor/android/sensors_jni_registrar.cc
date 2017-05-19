// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/android/sensors_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "device/generic_sensor/platform_sensor_android.h"

namespace device {
namespace android {
namespace {

const base::android::RegistrationMethod kRegisteredMethods[] = {
    {"PlatformSensorAndroid", device::PlatformSensorAndroid::RegisterJNI},
};

}  // namespace

bool RegisterSensorsJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kRegisteredMethods,
                               arraysize(kRegisteredMethods));
}

}  // namespace android
}  // namespace device
