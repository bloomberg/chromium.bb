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
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.HashMap;
import java.util.HashSet;
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

    private long mNativeBluetoothDeviceAndroid;
    final Wrappers.BluetoothDeviceWrapper mDevice;
    private HashSet<String> mUuidsFromScan;
    Wrappers.BluetoothGattWrapper mBluetoothGatt;
    private final BluetoothGattCallbackImpl mBluetoothGattCallbackImpl;
    final HashMap<Wrappers.BluetoothGattCharacteristicWrapper,
            ChromeBluetoothRemoteGattCharacteristic> mWrapperToChromeCharacteristicsMap;
    final HashMap<Wrappers.BluetoothGattDescriptorWrapper, ChromeBluetoothRemoteGattDescriptor>
            mWrapperToChromeDescriptorsMap;

    private ChromeBluetoothDevice(
            long nativeBluetoothDeviceAndroid, Wrappers.BluetoothDeviceWrapper deviceWrapper) {
        mNativeBluetoothDeviceAndroid = nativeBluetoothDeviceAndroid;
        mDevice = deviceWrapper;
        mUuidsFromScan = new HashSet<String>();
        mBluetoothGattCallbackImpl = new BluetoothGattCallbackImpl();
        mWrapperToChromeCharacteristicsMap =
                new HashMap<Wrappers.BluetoothGattCharacteristicWrapper,
                        ChromeBluetoothRemoteGattCharacteristic>();
        mWrapperToChromeDescriptorsMap = new HashMap<Wrappers.BluetoothGattDescriptorWrapper,
                ChromeBluetoothRemoteGattDescriptor>();
        Log.v(TAG, "ChromeBluetoothDevice created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothDeviceAndroidDestruction() {
        if (mBluetoothGatt != null) {
            mBluetoothGatt.close();
            mBluetoothGatt = null;
        }
        mNativeBluetoothDeviceAndroid = 0;
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
        if (uuidsFromScan == null) {
            return false;
        }
        boolean uuidsUpdated = false;
        for (ParcelUuid uuid : uuidsFromScan) {
            uuidsUpdated |= mUuidsFromScan.add(uuid.toString());
        }
        return uuidsUpdated;
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
        // TODO(scheib): return merged list of UUIDs from scan results and,
        // after a device is connected, discoverServices. crbug.com/508648
        return mUuidsFromScan.toArray(new String[mUuidsFromScan.size()]);
    }

    // Implements BluetoothDeviceAndroid::CreateGattConnectionImpl.
    @CalledByNative
    private void createGattConnectionImpl(Context context) {
        Log.i(TAG, "connectGatt");

        if (mBluetoothGatt != null) mBluetoothGatt.close();

        // autoConnect set to false as under experimentation using autoConnect failed to complete
        // connections.
        mBluetoothGatt =
                mDevice.connectGatt(context, false /* autoConnect */, mBluetoothGattCallbackImpl);
    }

    // Implements BluetoothDeviceAndroid::DisconnectGatt.
    @CalledByNative
    private void disconnectGatt() {
        Log.i(TAG, "BluetoothGatt.disconnect");
        if (mBluetoothGatt != null) mBluetoothGatt.disconnect();
    }

    // Implements BluetoothDeviceAndroid::GetDeviceName.
    @CalledByNative
    private String getDeviceName() {
        return mDevice.getName();
    }

    // Implements callbacks related to a GATT connection.
    private class BluetoothGattCallbackImpl extends Wrappers.BluetoothGattCallbackWrapper {
        @Override
        public void onConnectionStateChange(final int status, final int newState) {
            Log.i(TAG, "onConnectionStateChange status:%d newState:%s", status,
                    (newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED)
                            ? "Connected"
                            : "Disconnected");
            if (newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED) {
                mBluetoothGatt.discoverServices();
            } else if (newState == android.bluetooth.BluetoothProfile.STATE_DISCONNECTED) {
                if (mBluetoothGatt != null) {
                    mBluetoothGatt.close();
                    mBluetoothGatt = null;
                }
            }
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (mNativeBluetoothDeviceAndroid != 0) {
                        nativeOnConnectionStateChange(mNativeBluetoothDeviceAndroid, status,
                                newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED);
                    }
                }
            });
        }

        @Override
        public void onServicesDiscovered(final int status) {
            Log.i(TAG, "onServicesDiscovered status:%d==%s", status,
                    status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (mNativeBluetoothDeviceAndroid != 0) {
                        // TODO(crbug.com/576906): Update or replace existing GATT objects if they
                        //                         change after initial discovery.
                        for (Wrappers.BluetoothGattServiceWrapper service :
                                mBluetoothGatt.getServices()) {
                            // Create an adapter unique service ID. getInstanceId only differs
                            // between service instances with the same UUID on this device.
                            String serviceInstanceId = getAddress() + "/"
                                    + service.getUuid().toString() + "," + service.getInstanceId();
                            nativeCreateGattRemoteService(
                                    mNativeBluetoothDeviceAndroid, serviceInstanceId, service);
                        }
                        nativeOnGattServicesDiscovered(mNativeBluetoothDeviceAndroid);
                    }
                }
            });
        }

        @Override
        public void onCharacteristicChanged(
                final Wrappers.BluetoothGattCharacteristicWrapper characteristic) {
            Log.i(TAG, "device onCharacteristicChanged.");
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic =
                            mWrapperToChromeCharacteristicsMap.get(characteristic);
                    if (chromeCharacteristic == null) {
                        // Android events arriving with no Chrome object is expected rarely only
                        // when the event races object destruction.
                        Log.v(TAG, "onCharacteristicChanged when chromeCharacteristic == null.");
                    } else {
                        chromeCharacteristic.onCharacteristicChanged();
                    }
                }
            });
        }

        @Override
        public void onCharacteristicRead(
                final Wrappers.BluetoothGattCharacteristicWrapper characteristic,
                final int status) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic =
                            mWrapperToChromeCharacteristicsMap.get(characteristic);
                    if (chromeCharacteristic == null) {
                        // Android events arriving with no Chrome object is expected rarely: only
                        // when the event races object destruction.
                        Log.v(TAG, "onCharacteristicRead when chromeCharacteristic == null.");
                    } else {
                        chromeCharacteristic.onCharacteristicRead(status);
                    }
                }
            });
        }

        @Override
        public void onCharacteristicWrite(
                final Wrappers.BluetoothGattCharacteristicWrapper characteristic,
                final int status) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic =
                            mWrapperToChromeCharacteristicsMap.get(characteristic);
                    if (chromeCharacteristic == null) {
                        // Android events arriving with no Chrome object is expected rarely: only
                        // when the event races object destruction.
                        Log.v(TAG, "onCharacteristicWrite when chromeCharacteristic == null.");
                    } else {
                        chromeCharacteristic.onCharacteristicWrite(status);
                    }
                }
            });
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:

    // Binds to BluetoothDeviceAndroid::OnConnectionStateChange.
    private native void nativeOnConnectionStateChange(
            long nativeBluetoothDeviceAndroid, int status, boolean connected);

    // Binds to BluetoothDeviceAndroid::CreateGattRemoteService.
    // TODO(http://crbug.com/505554): Replace 'Object' with specific type when JNI fixed.
    private native void nativeCreateGattRemoteService(long nativeBluetoothDeviceAndroid,
            String instanceId, Object bluetoothGattServiceWrapper);

    // Binds to BluetoothDeviceAndroid::GattServicesDiscovered.
    private native void nativeOnGattServicesDiscovered(long nativeBluetoothDeviceAndroid);
}
