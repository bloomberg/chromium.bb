// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothGattService as necessary
 * for C++ device::BluetoothRemoteGattServiceAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattServiceAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothRemoteGattService {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothRemoteGattServiceAndroid;
    final Wrappers.BluetoothGattServiceWrapper mService;

    private ChromeBluetoothRemoteGattService(long nativeBluetoothRemoteGattServiceAndroid,
            Wrappers.BluetoothGattServiceWrapper serviceWrapper) {
        mNativeBluetoothRemoteGattServiceAndroid = nativeBluetoothRemoteGattServiceAndroid;
        mService = serviceWrapper;
        Log.v(TAG, "ChromeBluetoothRemoteGattService created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothRemoteGattServiceAndroidDestruction() {
        mNativeBluetoothRemoteGattServiceAndroid = 0;
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattServiceAndroid methods implemented in java:

    // Implements BluetoothRemoteGattServiceAndroid::Create.
    // 'Object' type must be used because inner class Wrappers.BluetoothGattServiceWrapper reference
    // is not handled by jni_generator.py JavaToJni. http://crbug.com/505554
    @CalledByNative
    private static ChromeBluetoothRemoteGattService create(
            long nativeBluetoothRemoteGattServiceAndroid, Object serviceWrapper) {
        return new ChromeBluetoothRemoteGattService(nativeBluetoothRemoteGattServiceAndroid,
                (Wrappers.BluetoothGattServiceWrapper) serviceWrapper);
    }

    // Implements BluetoothRemoteGattServiceAndroid::GetUUID.
    @CalledByNative
    private String getUUID() {
        return mService.getUuid().toString();
    }

    // Implements BluetoothRemoteGattServiceAndroid::EnsureCharacteristicsCreated
    @CalledByNative
    private void ensureCharacteristicsCreated() {
        List<Wrappers.BluetoothGattCharacteristicWrapper> characteristics =
                mService.getCharacteristics();
        for (Wrappers.BluetoothGattCharacteristicWrapper characteristic : characteristics) {
            // Create a unique characteristic ID. getInstanceId only differs between characteristic
            // instances with the same UUID.
            // TODO(scheib): Make instance IDs unique to the whole adapter. http://crbug.com/546747
            String characteristicInstanceId =
                    characteristic.getUuid().toString() + characteristic.getInstanceId();
            nativeCreateGattRemoteCharacteristic(mNativeBluetoothRemoteGattServiceAndroid,
                    characteristicInstanceId, characteristic);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:
    // Binds to BluetoothRemoteGattServiceAndroid::CreateGattRemoteCharacteristic.
    // 'Object' type must be used for |bluetoothGattCarachteristicWrapper| because inner class
    // Wrappers.BluetoothGattCharacteristicWrapper reference is not handled by jni_generator.py
    // JavaToJni.
    // http://crbug.com/505554
    private native void nativeCreateGattRemoteCharacteristic(
            long nativeBluetoothRemoteGattServiceAndroid, String instanceId,
            Object bluetoothGattCarachteristicWrapper);
}
