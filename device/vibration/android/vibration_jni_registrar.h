// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VIBRATION_ANDROID_VIBRATION_JNI_REGISTRAR_H_
#define DEVICE_VIBRATION_ANDROID_VIBRATION_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/vibration/vibration_export.h"

namespace device {
namespace android {

DEVICE_VIBRATION_EXPORT bool RegisterVibrationJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_VIBRATION_ANDROID_VIBRATION_JNI_REGISTRAR_H_
