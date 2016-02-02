// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;
import java.util.UUID;

/**
 * Exposes android.bluetooth.BluetoothGattCharacteristic as necessary
 * for C++ device::BluetoothRemoteGattCharacteristicAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattCharacteristicAndroid.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
final class ChromeBluetoothRemoteGattCharacteristic {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothRemoteGattCharacteristicAndroid;
    final Wrappers.BluetoothGattCharacteristicWrapper mCharacteristic;
    final String mInstanceId;
    final ChromeBluetoothDevice mChromeDevice;

    private ChromeBluetoothRemoteGattCharacteristic(
            long nativeBluetoothRemoteGattCharacteristicAndroid,
            Wrappers.BluetoothGattCharacteristicWrapper characteristicWrapper, String instanceId,
            ChromeBluetoothDevice chromeDevice) {
        mNativeBluetoothRemoteGattCharacteristicAndroid =
                nativeBluetoothRemoteGattCharacteristicAndroid;
        mCharacteristic = characteristicWrapper;
        mInstanceId = instanceId;
        mChromeDevice = chromeDevice;

        mChromeDevice.mWrapperToChromeCharacteristicsMap.put(characteristicWrapper, this);

        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothRemoteGattCharacteristicAndroidDestruction() {
        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic Destroyed.");
        if (mChromeDevice.mBluetoothGatt != null) {
            mChromeDevice.mBluetoothGatt.setCharacteristicNotification(mCharacteristic, false);
        }
        mNativeBluetoothRemoteGattCharacteristicAndroid = 0;
        mChromeDevice.mWrapperToChromeCharacteristicsMap.remove(mCharacteristic);
    }

    void onCharacteristicChanged() {
        Log.i(TAG, "onCharacteristicChanged");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            nativeOnChanged(
                    mNativeBluetoothRemoteGattCharacteristicAndroid, mCharacteristic.getValue());
        }
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
            Object bluetoothGattCharacteristicWrapper, String instanceId,
            ChromeBluetoothDevice chromeDevice) {
        return new ChromeBluetoothRemoteGattCharacteristic(
                nativeBluetoothRemoteGattCharacteristicAndroid,
                (Wrappers.BluetoothGattCharacteristicWrapper) bluetoothGattCharacteristicWrapper,
                instanceId, chromeDevice);
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
        // Verify properties first, to provide clearest error log.
        int properties = mCharacteristic.getProperties();
        boolean hasNotify = (properties & BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0;
        boolean hasIndicate = (properties & BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0;
        if (!hasNotify && !hasIndicate) {
            Log.v(TAG, "startNotifySession failed! Characteristic needs NOTIFY or INDICATE.");
            return false;
        }

        // Find config descriptor.
        Wrappers.BluetoothGattDescriptorWrapper clientCharacteristicConfigurationDescriptor =
                mCharacteristic.getDescriptor(UUID.fromString(
                        "00002902-0000-1000-8000-00805F9B34FB" /* Config's standard UUID*/));
        if (clientCharacteristicConfigurationDescriptor == null) {
            Log.v(TAG, "startNotifySession config descriptor failed!");
            return false;
        }

        // Request Android route onCharacteristicChanged notifications for this characteristic.
        if (!mChromeDevice.mBluetoothGatt.setCharacteristicNotification(mCharacteristic, true)) {
            Log.i(TAG, "startNotifySession setCharacteristicNotification failed.");
            return false;
        }

        // Enable notification on remote device's characteristic:
        if (!clientCharacteristicConfigurationDescriptor.setValue(hasNotify
                            ? BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            : BluetoothGattDescriptor.ENABLE_INDICATION_VALUE)) {
            Log.v(TAG, "startNotifySession descriptor setValue failed!");
            return false;
        }
        Log.v(TAG, hasNotify ? "startNotifySession NOTIFY." : "startNotifySession INDICATE.");

        if (!mChromeDevice.mBluetoothGatt.writeDescriptor(
                    clientCharacteristicConfigurationDescriptor)) {
            Log.i(TAG, "startNotifySession writeDescriptor failed!");
            return false;
        }

        return true;
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic.
    @CalledByNative
    private boolean readRemoteCharacteristic() {
        if (!mChromeDevice.mBluetoothGatt.readCharacteristic(mCharacteristic)) {
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
        if (!mChromeDevice.mBluetoothGatt.writeCharacteristic(mCharacteristic)) {
            Log.i(TAG, "writeRemoteCharacteristic writeCharacteristic failed.");
            return false;
        }
        return true;
    }

    // Creates objects for all descriptors. Designed only to be called by
    // BluetoothRemoteGattCharacteristicAndroid::EnsureDescriptorsCreated.
    @CalledByNative
    private void createDescriptors() {
        List<Wrappers.BluetoothGattDescriptorWrapper> descriptors =
                mCharacteristic.getDescriptors();
        for (Wrappers.BluetoothGattDescriptorWrapper descriptor : descriptors) {
            // Create an adapter unique descriptor ID.
            // TODO(crbug.com/576900) Unique descriptorInstanceId duplicate UUID values.
            String descriptorInstanceId = mInstanceId + "/" + descriptor.getUuid().toString();
            nativeCreateGattRemoteDescriptor(mNativeBluetoothRemoteGattCharacteristicAndroid,
                    descriptorInstanceId, descriptor, mChromeDevice);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:

    // Binds to BluetoothRemoteGattCharacteristicAndroid::OnChanged.
    native void nativeOnChanged(long nativeBluetoothRemoteGattCharacteristicAndroid, byte[] value);

    // Binds to BluetoothRemoteGattCharacteristicAndroid::OnRead.
    native void nativeOnRead(
            long nativeBluetoothRemoteGattCharacteristicAndroid, int status, byte[] value);

    // Binds to BluetoothRemoteGattCharacteristicAndroid::OnWrite.
    native void nativeOnWrite(long nativeBluetoothRemoteGattCharacteristicAndroid, int status);

    // Binds to BluetoothRemoteGattCharacteristicAndroid::CreateGattRemoteDescriptor.
    // TODO(http://crbug.com/505554): Replace 'Object' with specific type when JNI fixed.
    private native void nativeCreateGattRemoteDescriptor(
            long nativeBluetoothRemoteGattCharacteristicAndroid, String instanceId,
            Object bluetoothGattDescriptorWrapper, Object chromeBluetoothDevice);
}
