// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_ANDROID_GAMEPAD_JNI_REGISTRAR_H_
#define DEVICE_GAMEPAD_ANDROID_GAMEPAD_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/gamepad/gamepad_export.h"

namespace device {
namespace android {

// Registers C++ methods in device/gamepad classes with JNI.
// See https://www.chromium.org/developers/design-documents/android-jni
//
// Must be called before classes in the Gamepad module are used.
DEVICE_GAMEPAD_EXPORT bool RegisterGamepadJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_GAMEPAD_ANDROID_GAMEPAD_JNI_REGISTRAR_H_
