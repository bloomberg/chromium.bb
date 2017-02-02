// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSOR_ANDROID_DEVICE_SENSOR_JNI_REGISTRAR_H_
#define DEVICE_SENSOR_ANDROID_DEVICE_SENSOR_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/sensors/device_sensor_export.h"

namespace device {
namespace android {

bool DEVICE_SENSOR_EXPORT RegisterDeviceSensorJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_SENSOR_ANDROID_DEVICE_SENSOR_JNI_REGISTRAR_H_
