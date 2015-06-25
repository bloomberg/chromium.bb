// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/android/bluetooth_adapter_wrapper.h"

#include "base/android/jni_android.h"
#include "jni/BluetoothAdapterWrapper_jni.h"

using base::android::AttachCurrentThread;

namespace device {

// static
ScopedJavaLocalRef<jobject>
BluetoothAdapterWrapper::CreateWithDefaultAdapter() {
  return Java_BluetoothAdapterWrapper_createWithDefaultAdapter(
      AttachCurrentThread(), base::android::GetApplicationContext());
}

// static
bool BluetoothAdapterWrapper::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // From BluetoothAdapterWrapper_jni.h
}

}  // namespace device
