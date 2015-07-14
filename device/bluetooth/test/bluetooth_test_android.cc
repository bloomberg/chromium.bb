// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_android.h"

#include "base/logging.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "jni/Fakes_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace device {

BluetoothTestAndroid::BluetoothTestAndroid() {
}

BluetoothTestAndroid::~BluetoothTestAndroid() {
}

void BluetoothTestAndroid::SetUp() {
  // Register in SetUp so that ASSERT can be used.
  ASSERT_TRUE(RegisterNativesImpl(AttachCurrentThread()));
}

void BluetoothTestAndroid::InitWithDefaultAdapter() {
  adapter_ =
      BluetoothAdapterAndroid::Create(
          BluetoothAdapterWrapper_CreateWithDefaultAdapter().obj()).get();
}

void BluetoothTestAndroid::InitWithoutDefaultAdapter() {
  adapter_ = BluetoothAdapterAndroid::Create(NULL).get();
}

void BluetoothTestAndroid::InitWithFakeAdapter() {
  j_fake_bluetooth_adapter_.Reset(
      Java_FakeBluetoothAdapter_create(AttachCurrentThread()));

  adapter_ =
      BluetoothAdapterAndroid::Create(j_fake_bluetooth_adapter_.obj()).get();
}

void BluetoothTestAndroid::DiscoverLowEnergyDevice(int device_ordinal) {
  Java_FakeBluetoothAdapter_discoverLowEnergyDevice(
      AttachCurrentThread(), j_fake_bluetooth_adapter_.obj(), device_ordinal);
}

}  // namespace device
