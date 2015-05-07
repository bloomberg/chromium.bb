// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ANDROID_BLUETOOTH_JNI_REGISTRAR_H_
#define DEVICE_BLUETOOTH_ANDROID_BLUETOOTH_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/bluetooth/bluetooth_export.h"

namespace device {
namespace android {

// Registers C++ methods in device/bluetooth classes with JNI.
// See https://www.chromium.org/developers/design-documents/android-jni
//
// Must be called before classes in the Bluetooth module are used.
DEVICE_BLUETOOTH_EXPORT bool RegisterBluetoothJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_BLUETOOTH_ANDROID_BLUETOOTH_JNI_REGISTRAR_H_
