// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.BluetoothAdapter;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.Log;

/**
 * Exposes android.bluetooth.BluetoothAdapter as necessary for C++
 * device::BluetoothAdapterAndroid, which implements the cross platform
 * device::BluetoothAdapter.
 */
@JNINamespace("device")
final class ChromeBluetoothAdapter {
    private static final String TAG = "cr.Bluetooth";

    private Wrappers.BluetoothAdapterWrapper mAdapter;

    /**
     * Constructs a ChromeBluetoothAdapter.
     * @param adapterWrapper Wraps the default android.bluetooth.BluetoothAdapter,
     *                       but may be either null if an adapter is not available
     *                       or a fake for testing.
     */
    private ChromeBluetoothAdapter(Wrappers.BluetoothAdapterWrapper adapterWrapper) {
        mAdapter = adapterWrapper;
        if (adapterWrapper == null) {
            Log.i(TAG, "ChromeBluetoothAdapter created with no adapterWrapper.");
        } else {
            Log.i(TAG, "ChromeBluetoothAdapter created with provided adapterWrapper.");
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterAndroid methods implemented in java:

    // Implements BluetoothAdapterAndroid::Create.
    // 'Object' type must be used because inner class Wrappers.BluetoothAdapterWrapper reference is
    // not handled by jni_generator.py JavaToJni. http://crbug.com/505554
    @CalledByNative
    public static ChromeBluetoothAdapter create(Object adapterWrapper) {
        return new ChromeBluetoothAdapter((Wrappers.BluetoothAdapterWrapper) adapterWrapper);
    }

    // Implements BluetoothAdapterAndroid::GetAddress.
    @CalledByNative
    private String getAddress() {
        if (isPresent()) {
            return mAdapter.getAddress();
        } else {
            return "";
        }
    }

    // Implements BluetoothAdapterAndroid::GetName.
    @CalledByNative
    private String getName() {
        if (isPresent()) {
            return mAdapter.getName();
        } else {
            return "";
        }
    }

    // Implements BluetoothAdapterAndroid::IsPresent.
    @CalledByNative
    private boolean isPresent() {
        return mAdapter != null;
    }

    // Implements BluetoothAdapterAndroid::IsPowered.
    @CalledByNative
    private boolean isPowered() {
        return isPresent() && mAdapter.isEnabled();
    }

    // Implements BluetoothAdapterAndroid::IsDiscoverable.
    @CalledByNative
    private boolean isDiscoverable() {
        return isPresent()
                && mAdapter.getScanMode() == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
    }

    // Implements BluetoothAdapterAndroid::IsDiscovering.
    @CalledByNative
    private boolean isDiscovering() {
        return isPresent() && mAdapter.isDiscovering();
    }
}
