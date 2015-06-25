// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ANDROID_BLUETOOTH_ADAPTER_WRAPPER_H_
#define DEVICE_BLUETOOTH_ANDROID_BLUETOOTH_ADAPTER_WRAPPER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "device/bluetooth/bluetooth_export.h"

using base::android::ScopedJavaLocalRef;

namespace device {

// Wraps Java class BluetoothAdapterWrapper.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterWrapper {
 public:
  // Calls Java: BluetoothAdapterWrapper.createWithDefaultAdapter().
  static ScopedJavaLocalRef<jobject> CreateWithDefaultAdapter();

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_ANDROID_BLUETOOTH_ADAPTER_WRAPPER_H_
