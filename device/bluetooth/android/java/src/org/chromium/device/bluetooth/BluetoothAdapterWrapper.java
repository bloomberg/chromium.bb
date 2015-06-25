// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.pm.PackageManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.Log;

/**
 * Wraps android.bluetooth.BluetoothAdapter, pasing through to a provided
 * object. This indirection enables fake implementations when running tests.
 */
@JNINamespace("device")
public class BluetoothAdapterWrapper {
    private static final String TAG = "cr.Bluetooth";
    private final BluetoothAdapter mAdapter;

    /***
     * Creates a BluetoothAdapterWrapper using the default android.bluetooth.BluetoothAdapter. May
     * fail if the default adapter is not available or if the application does not have sufficient
     * permissions.
     */
    @CalledByNative
    public static BluetoothAdapterWrapper createWithDefaultAdapter(Context context) {
        final boolean hasPermissions =
                context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH)
                        == PackageManager.PERMISSION_GRANTED
                && context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH_ADMIN)
                        == PackageManager.PERMISSION_GRANTED;
        if (!hasPermissions) {
            Log.w(TAG, "BluetoothAdapterWrapper.create failed: Lacking Bluetooth permissions.");
            return null;
        }

        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) {
            Log.i(TAG, "BluetoothAdapterWrapper.create failed: Default adapter not found.");
            return null;
        } else {
            Log.i(TAG, "BluetoothAdapterWrapper created with default adapter.");
            return new BluetoothAdapterWrapper(adapter);
        }
    }

    public BluetoothAdapterWrapper(BluetoothAdapter adapter) {
        mAdapter = adapter;
    }

    public boolean isEnabled() {
        return mAdapter.isEnabled();
    }

    public String getAddress() {
        return mAdapter.getAddress();
    }

    public String getName() {
        return mAdapter.getName();
    }

    public int getScanMode() {
        return mAdapter.getScanMode();
    }

    public boolean isDiscovering() {
        return mAdapter.isDiscovering();
    }
}
