// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_POWER_SAVE_BLOCKER_ANDROID_POWER_SAVE_BLOCKER_JNI_REGISTRAR_H_
#define DEVICE_POWER_SAVE_BLOCKER_ANDROID_POWER_SAVE_BLOCKER_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/power_save_blocker/power_save_blocker_export.h"

namespace device {
namespace android {

// Registers C++ methods in device/power_save_blocker classes with JNI.
// See https://www.chromium.org/developers/design-documents/android-jni
//
// Must be called before classes in the Bluetooth module are used.
DEVICE_POWER_SAVE_BLOCKER_EXPORT bool RegisterPowerSaveBlockerJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_POWER_SAVE_BLOCKER_ANDROID_POWER_SAVE_BLOCKER_JNI_REGISTRAR_H_
