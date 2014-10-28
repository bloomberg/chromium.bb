// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BATTERY_ANDROID_BATTERY_JNI_REGISTRAR_H_
#define DEVICE_BATTERY_ANDROID_BATTERY_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/battery/battery_export.h"

namespace device {
namespace android {

DEVICE_BATTERY_EXPORT bool RegisterBatteryJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_BATTERY_ANDROID_BATTERY_JNI_REGISTRAR_H_
