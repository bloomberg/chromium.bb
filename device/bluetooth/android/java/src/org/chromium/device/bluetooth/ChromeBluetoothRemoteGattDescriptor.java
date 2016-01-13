// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.bluetooth.BluetoothGattDescriptor as necessary
 * for C++ device::BluetoothRemoteGattDescriptorAndroid.
 *
 * Lifetime is controlled by device::BluetoothRemoteGattDescriptorAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothRemoteGattDescriptor {
    private static final String TAG = "Bluetooth";

    // TODO(scheib): Will need c++ pointer eventually:
    //     private long mNativeBluetoothRemoteGattDescriptorAndroid;
    final Wrappers.BluetoothGattDescriptorWrapper mDescriptor;
    final ChromeBluetoothDevice mChromeDevice;

    private ChromeBluetoothRemoteGattDescriptor(
            // TODO(scheib): Will need c++ pointer eventually:
            //    long nativeBluetoothRemoteGattDescriptorAndroid,
            Wrappers.BluetoothGattDescriptorWrapper descriptorWrapper,
            ChromeBluetoothDevice chromeDevice) {
        // TODO(scheib): Will need c++ pointer eventually:
        //    mNativeBluetoothRemoteGattDescriptorAndroid =
        //    nativeBluetoothRemoteGattDescriptorAndroid;
        mDescriptor = descriptorWrapper;
        mChromeDevice = chromeDevice;

        mChromeDevice.mWrapperToChromeDescriptorsMap.put(descriptorWrapper, this);

        Log.v(TAG, "ChromeBluetoothRemoteGattDescriptor created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothRemoteGattDescriptorAndroidDestruction() {
        Log.v(TAG, "ChromeBluetoothRemoteGattDescriptor Destroyed.");
        // TODO(scheib): Will need c++ pointer eventually:
        //    mNativeBluetoothRemoteGattDescriptorAndroid = 0;
        mChromeDevice.mWrapperToChromeDescriptorsMap.remove(mDescriptor);
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattDescriptorAndroid methods implemented in java:

    // Implements BluetoothRemoteGattDescriptorAndroid::Create.
    // TODO(http://crbug.com/505554): Replace 'Object' with specific type when JNI fixed.
    @CalledByNative
    private static ChromeBluetoothRemoteGattDescriptor create(
            // TODO(scheib): Will need c++ pointer eventually:
            //    long nativeBluetoothRemoteGattDescriptorAndroid,
            Object bluetoothGattDescriptorWrapper, ChromeBluetoothDevice chromeDevice) {
        return new ChromeBluetoothRemoteGattDescriptor(
                // TODO(scheib): Will need c++ pointer eventually:
                //    nativeBluetoothRemoteGattDescriptorAndroid,
                (Wrappers.BluetoothGattDescriptorWrapper) bluetoothGattDescriptorWrapper,
                chromeDevice);
    }

    // Implements BluetoothRemoteGattDescriptorAndroid::GetUUID.
    @CalledByNative
    private String getUUID() {
        return mDescriptor.getUuid().toString();
    }
}
