// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_ANDROID_SENSORS_JNI_REGISTRAR_H_
#define DEVICE_GENERIC_SENSOR_ANDROID_SENSORS_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/generic_sensor/generic_sensor_export.h"

namespace device {
namespace android {

// Registers C++ methods in device/generic_sensor classes with JNI.
// See https://www.chromium.org/developers/design-documents/android-jni
//
// Must be called before classes in the Sensors module are used.
bool DEVICE_GENERIC_SENSOR_EXPORT RegisterSensorsJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_ANDROID_SENSORS_JNI_REGISTRAR_H_
