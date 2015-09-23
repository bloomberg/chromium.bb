// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.os.Build;
import android.os.ParcelUuid;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothDevice as necessary for C++
 * device::BluetoothDeviceAndroid.
 *
 * Lifetime is controlled by device::BluetoothDeviceAndroid.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
final class ChromeBluetoothDevice {
    private static final String TAG = "Bluetooth";

    private final long mNativeBluetoothDeviceAndroid;
    final Wrappers.BluetoothDeviceWrapper mDevice;
    private List<ParcelUuid> mUuidsFromScan;
    Wrappers.BluetoothGattWrapper mBluetoothGatt;
    private final BluetoothGattCallbackImpl mBluetoothGattCallbackImpl;

    private ChromeBluetoothDevice(
            long nativeBluetoothDeviceAndroid, Wrappers.BluetoothDeviceWrapper deviceWrapper) {
        mNativeBluetoothDeviceAndroid = nativeBluetoothDeviceAndroid;
        mDevice = deviceWrapper;
        mBluetoothGattCallbackImpl = new BluetoothGattCallbackImpl();
        Log.v(TAG, "ChromeBluetoothDevice created.");
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothDeviceAndroid methods implemented in java:

    // Implements BluetoothDeviceAndroid::Create.
    // 'Object' type must be used because inner class Wrappers.BluetoothDeviceWrapper reference is
    // not handled by jni_generator.py JavaToJni. http://crbug.com/505554
    @CalledByNative
    private static ChromeBluetoothDevice create(
            long nativeBluetoothDeviceAndroid, Object deviceWrapper) {
        return new ChromeBluetoothDevice(
                nativeBluetoothDeviceAndroid, (Wrappers.BluetoothDeviceWrapper) deviceWrapper);
    }

    // Implements BluetoothDeviceAndroid::UpdateAdvertisedUUIDs.
    @CalledByNative
    private boolean updateAdvertisedUUIDs(List<ParcelUuid> uuidsFromScan) {
        if ((mUuidsFromScan == null && uuidsFromScan == null)
                || (mUuidsFromScan != null && mUuidsFromScan.equals(uuidsFromScan))) {
            return false;
        }
        mUuidsFromScan = uuidsFromScan;
        return true;
    }

    // Implements BluetoothDeviceAndroid::GetBluetoothClass.
    @CalledByNative
    private int getBluetoothClass() {
        return mDevice.getBluetoothClass_getDeviceClass();
    }

    // Implements BluetoothDeviceAndroid::GetAddress.
    @CalledByNative
    private String getAddress() {
        return mDevice.getAddress();
    }

    // Implements BluetoothDeviceAndroid::IsPaired.
    @CalledByNative
    private boolean isPaired() {
        return mDevice.getBondState() == BluetoothDevice.BOND_BONDED;
    }

    // Implements BluetoothDeviceAndroid::GetUUIDs.
    @CalledByNative
    private String[] getUuids() {
        int uuidCount = (mUuidsFromScan != null) ? mUuidsFromScan.size() : 0;
        String[] string_array = new String[uuidCount];
        for (int i = 0; i < uuidCount; i++) {
            string_array[i] = mUuidsFromScan.get(i).toString();
        }

        // TODO(scheib): return merged list of UUIDs from scan results and,
        // after a device is connected, discoverServices. crbug.com/508648

        return string_array;
    }

    // Implements BluetoothDeviceAndroid::CreateGattConnectionImpl.
    @CalledByNative
    private void createGattConnectionImpl(Context context) {
        Log.i(TAG, "connectGatt");
        // autoConnect set to false as under experimentation using autoConnect failed to complete
        // connections.
        mBluetoothGatt =
                mDevice.connectGatt(context, false /* autoConnect */, mBluetoothGattCallbackImpl);
    }

    // Implements BluetoothDeviceAndroid::DisconnectGatt.
    @CalledByNative
    private void disconnectGatt() {
        Log.i(TAG, "BluetoothGatt.disconnect");
        mBluetoothGatt.disconnect();
    }

    // Implements BluetoothDeviceAndroid::GetDeviceName.
    @CalledByNative
    private String getDeviceName() {
        return mDevice.getName();
    }

    // Implements callbacks related to a GATT connection.
    private class BluetoothGattCallbackImpl extends Wrappers.BluetoothGattCallbackWrapper {
        @Override
        public void onConnectionStateChange(int status, int newState) {
            Log.i(TAG, "onConnectionStateChange status:%d newState:%s", status,
                    (newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED)
                            ? "Connected"
                            : "Disconnected");
            nativeOnConnectionStateChange(mNativeBluetoothDeviceAndroid, status,
                    newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:

    // Binds to BluetoothDeviceAndroid::OnConnectionStateChange.
    private native void nativeOnConnectionStateChange(
            long nativeBluetoothDeviceAndroid, int status, boolean connected);
}
