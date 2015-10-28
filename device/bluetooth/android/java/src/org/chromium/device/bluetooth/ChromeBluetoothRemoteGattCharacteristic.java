// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.bluetooth.BluetoothGattCharacteristic as necessary
 * for C++ device::BluetoothRemoteGattCharacteristicAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattCharacteristicAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothRemoteGattCharacteristic {
    private static final String TAG = "Bluetooth";

    final Wrappers.BluetoothGattCharacteristicWrapper mCharacteristic;

    private ChromeBluetoothRemoteGattCharacteristic(
            Wrappers.BluetoothGattCharacteristicWrapper characteristicWrapper) {
        mCharacteristic = characteristicWrapper;
        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic created.");
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattCharacteristicAndroid methods implemented in java:

    // Implements BluetoothRemoteGattCharacteristicAndroid::Create.
    // 'Object' type must be used because inner class Wrappers.BluetoothGattCharacteristicWrapper
    // reference is not handled by jni_generator.py JavaToJni. http://crbug.com/505554
    @CalledByNative
    private static ChromeBluetoothRemoteGattCharacteristic create(Object characteristicWrapper) {
        return new ChromeBluetoothRemoteGattCharacteristic(
                (Wrappers.BluetoothGattCharacteristicWrapper) characteristicWrapper);
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::GetUUID.
    @CalledByNative
    private String getUUID() {
        return mCharacteristic.getUuid().toString();
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::GetProperties.
    @CalledByNative
    private int getProperties() {
        // TODO(scheib): Must read Extended Properties Descriptor. crbug.com/548449
        return mCharacteristic.getProperties();
    }
}
