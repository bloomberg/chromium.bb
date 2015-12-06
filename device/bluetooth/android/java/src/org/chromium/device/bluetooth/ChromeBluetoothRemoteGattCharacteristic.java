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

    private long mNativeBluetoothRemoteGattCharacteristicAndroid;
    final Wrappers.BluetoothGattCharacteristicWrapper mCharacteristic;
    final ChromeBluetoothDevice mChromeBluetoothDevice;

    private ChromeBluetoothRemoteGattCharacteristic(
            long nativeBluetoothRemoteGattCharacteristicAndroid,
            Wrappers.BluetoothGattCharacteristicWrapper characteristicWrapper,
            ChromeBluetoothDevice chromeBluetoothDevice) {
        mNativeBluetoothRemoteGattCharacteristicAndroid =
                nativeBluetoothRemoteGattCharacteristicAndroid;
        mCharacteristic = characteristicWrapper;
        mChromeBluetoothDevice = chromeBluetoothDevice;

        mChromeBluetoothDevice.mWrapperToChromeCharacteristicsMap.put(characteristicWrapper, this);

        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothRemoteGattCharacteristicAndroidDestruction() {
        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic Destroyed.");
        mNativeBluetoothRemoteGattCharacteristicAndroid = 0;
        mChromeBluetoothDevice.mWrapperToChromeCharacteristicsMap.remove(mCharacteristic);
    }

    void onCharacteristicRead(int status) {
        Log.i(TAG, "onCharacteristicRead status:%d==%s", status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            nativeOnRead(mNativeBluetoothRemoteGattCharacteristicAndroid, status,
                    mCharacteristic.getValue());
        }
    }

    void onCharacteristicWrite(int status) {
        Log.i(TAG, "onCharacteristicWrite status:%d==%s", status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            nativeOnWrite(mNativeBluetoothRemoteGattCharacteristicAndroid, status);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattCharacteristicAndroid methods implemented in java:

    // Implements BluetoothRemoteGattCharacteristicAndroid::Create.
    // TODO(http://crbug.com/505554): Replace 'Object' with specific type when JNI fixed.
    @CalledByNative
    private static ChromeBluetoothRemoteGattCharacteristic create(
            long nativeBluetoothRemoteGattCharacteristicAndroid,
            Object bluetoothGattCarachteristicWrapper, Object chromeBluetoothDevice) {
        return new ChromeBluetoothRemoteGattCharacteristic(
                nativeBluetoothRemoteGattCharacteristicAndroid,
                (Wrappers.BluetoothGattCharacteristicWrapper) bluetoothGattCarachteristicWrapper,
                (ChromeBluetoothDevice) chromeBluetoothDevice);
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

    // Implements BluetoothRemoteGattCharacteristicAndroid::StartNotifySession.
    @CalledByNative
    private boolean startNotifySession() {
        if (!mChromeBluetoothDevice.mBluetoothGatt.setCharacteristicNotification(
                    mCharacteristic, true)) {
            Log.i(TAG, "startNotifySession setCharacteristicNotification failed.");
            return false;
        }
        Log.i(TAG, "startNotifySession.");
        return true;
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic.
    @CalledByNative
    private boolean readRemoteCharacteristic() {
        if (!mChromeBluetoothDevice.mBluetoothGatt.readCharacteristic(mCharacteristic)) {
            Log.i(TAG, "readRemoteCharacteristic readCharacteristic failed.");
            return false;
        }
        return true;
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic.
    @CalledByNative
    private boolean writeRemoteCharacteristic(byte[] value) {
        if (!mCharacteristic.setValue(value)) {
            Log.i(TAG, "writeRemoteCharacteristic setValue failed.");
            return false;
        }
        if (!mChromeBluetoothDevice.mBluetoothGatt.writeCharacteristic(mCharacteristic)) {
            Log.i(TAG, "writeRemoteCharacteristic writeCharacteristic failed.");
            return false;
        }
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:

    // Binds to BluetoothRemoteGattServiceAndroid::OnRead.
    native void nativeOnRead(
            long nativeBluetoothRemoteGattCharacteristicAndroid, int status, byte[] value);

    // Binds to BluetoothRemoteGattServiceAndroid::OnWrite.
    native void nativeOnWrite(long nativeBluetoothRemoteGattCharacteristicAndroid, int status);
}
