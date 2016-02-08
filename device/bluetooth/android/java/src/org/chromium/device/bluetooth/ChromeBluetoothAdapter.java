// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.Manifest;
import android.annotation.TargetApi;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.ScanSettings;
import android.os.Build;
import android.os.ParcelUuid;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothAdapter as necessary for C++
 * device::BluetoothAdapterAndroid, which implements the cross platform
 * device::BluetoothAdapter.
 *
 * Lifetime is controlled by device::BluetoothAdapterAndroid.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
final class ChromeBluetoothAdapter {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothAdapterAndroid;
    private Wrappers.BluetoothAdapterWrapper mAdapter;
    private int mNumDiscoverySessions;
    private ScanCallback mScanCallback;

    // ---------------------------------------------------------------------------------------------
    // Construction and handler for C++ object destruction.

    /**
     * Constructs a ChromeBluetoothAdapter.
     * @param nativeBluetoothAdapterAndroid Is the associated C++
     *                                      BluetoothAdapterAndroid pointer value.
     * @param adapterWrapper Wraps the default android.bluetooth.BluetoothAdapter,
     *                       but may be either null if an adapter is not available
     *                       or a fake for testing.
     */
    public ChromeBluetoothAdapter(
            long nativeBluetoothAdapterAndroid, Wrappers.BluetoothAdapterWrapper adapterWrapper) {
        mNativeBluetoothAdapterAndroid = nativeBluetoothAdapterAndroid;
        mAdapter = adapterWrapper;
        if (adapterWrapper == null) {
            Log.i(TAG, "ChromeBluetoothAdapter created with no adapterWrapper.");
        } else {
            Log.i(TAG, "ChromeBluetoothAdapter created with provided adapterWrapper.");
        }
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothAdapterAndroidDestruction() {
        stopScan();
        mNativeBluetoothAdapterAndroid = 0;
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterAndroid methods implemented in java:

    // Implements BluetoothAdapterAndroid::Create.
    // 'Object' type must be used for |adapterWrapper| because inner class
    // Wrappers.BluetoothAdapterWrapper reference is not handled by jni_generator.py JavaToJni.
    // http://crbug.com/505554
    @CalledByNative
    private static ChromeBluetoothAdapter create(
            long nativeBluetoothAdapterAndroid, Object adapterWrapper) {
        return new ChromeBluetoothAdapter(
                nativeBluetoothAdapterAndroid, (Wrappers.BluetoothAdapterWrapper) adapterWrapper);
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

    // Implements BluetoothAdapterAndroid::SetPowered.
    @CalledByNative
    private boolean setPowered(boolean powered) {
        if (powered) {
            return isPresent() && mAdapter.enable();
        } else {
            return isPresent() && mAdapter.disable();
        }
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
        return isPresent() && (mAdapter.isDiscovering() || mScanCallback != null);
    }

    // Implements BluetoothAdapterAndroid::AddDiscoverySession.
    @CalledByNative
    private boolean addDiscoverySession() {
        if (!isPowered()) {
            Log.d(TAG, "addDiscoverySession: Fails: !isPowered");
            return false;
        }

        mNumDiscoverySessions++;
        Log.d(TAG, "addDiscoverySession: Now %d sessions.", mNumDiscoverySessions);
        if (mNumDiscoverySessions > 1) {
            return true;
        }

        if (!startScan()) {
            mNumDiscoverySessions--;
            return false;
        }
        return true;
    }

    // Implements BluetoothAdapterAndroid::RemoveDiscoverySession.
    @CalledByNative
    private boolean removeDiscoverySession() {
        if (mNumDiscoverySessions == 0) {
            assert false;
            Log.w(TAG, "removeDiscoverySession: No scan in progress.");
            return false;
        }

        --mNumDiscoverySessions;

        if (mNumDiscoverySessions == 0) {
            Log.d(TAG, "removeDiscoverySession: Now 0 sessions. Stopping scan.");
            return stopScan();
        }

        Log.d(TAG, "removeDiscoverySession: Now %d sessions.", mNumDiscoverySessions);
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // Implementation details:

    /**
     * @return true if Chromium has permission to scan for Bluetooth devices.
     */
    private boolean canScan() {
        Wrappers.ContextWrapper context = mAdapter.getContext();
        return context.checkPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                || context.checkPermission(Manifest.permission.ACCESS_FINE_LOCATION);
    }

    /**
     * Starts a Low Energy scan.
     * @return True on success.
     */
    private boolean startScan() {
        Wrappers.BluetoothLeScannerWrapper scanner = mAdapter.getBluetoothLeScanner();

        if (scanner == null) {
            return false;
        }

        if (!canScan()) {
            return false;
        }

        // scanMode note: SCAN_FAILED_FEATURE_UNSUPPORTED is caused (at least on some devices) if
        // setReportDelay() is used or if SCAN_MODE_LOW_LATENCY isn't used.
        int scanMode = ScanSettings.SCAN_MODE_LOW_LATENCY;

        assert mScanCallback == null;
        mScanCallback = new ScanCallback();

        try {
            scanner.startScan(null /* filters */, scanMode, mScanCallback);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot start scan: " + e);
            mScanCallback = null;
            return false;
        } catch (IllegalStateException e) {
            Log.e(TAG, "Adapter is off. Cannot start scan: " + e);
            mScanCallback = null;
            return false;
        }
        return true;
    }

    /**
     * Stops the Low Energy scan.
     * @return True if a scan was in progress.
     */
    private boolean stopScan() {
        if (mScanCallback == null) {
            return false;
        }

        try {
            Wrappers.BluetoothLeScannerWrapper scanner = mAdapter.getBluetoothLeScanner();
            if (scanner != null) {
                scanner.stopScan(mScanCallback);
            }
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot stop scan: " + e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Adapter is off. Cannot stop scan: " + e);
        }
        mScanCallback = null;
        return true;
    }

    /**
     * Implements callbacks used during a Low Energy scan by notifying upon
     * devices discovered or detecting a scan failure.
     */
    private class ScanCallback extends Wrappers.ScanCallbackWrapper {
        @Override
        public void onBatchScanResult(List<Wrappers.ScanResultWrapper> results) {
            Log.v(TAG, "onBatchScanResults");
        }

        @Override
        public void onScanResult(int callbackType, Wrappers.ScanResultWrapper result) {
            Log.v(TAG, "onScanResult %d %s %s", callbackType, result.getDevice().getAddress(),
                    result.getDevice().getName());

            List<ParcelUuid> uuids = result.getScanRecord_getServiceUuids();

            nativeCreateOrUpdateDeviceOnScan(mNativeBluetoothAdapterAndroid,
                    result.getDevice().getAddress(), result.getDevice(), uuids);
        }

        @Override
        public void onScanFailed(int errorCode) {
            Log.w(TAG, "onScanFailed: %d", errorCode);
            nativeOnScanFailed(mNativeBluetoothAdapterAndroid);
            mNumDiscoverySessions = 0;
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterAndroid C++ methods declared for access from java:

    // Binds to BluetoothAdapterAndroid::OnScanFailed.
    private native void nativeOnScanFailed(long nativeBluetoothAdapterAndroid);

    // Binds to BluetoothAdapterAndroid::CreateOrUpdateDeviceOnScan.
    // 'Object' type must be used for |bluetoothDeviceWrapper| because inner class
    // Wrappers.BluetoothDeviceWrapper reference is not handled by jni_generator.py JavaToJni.
    // http://crbug.com/505554
    private native void nativeCreateOrUpdateDeviceOnScan(long nativeBluetoothAdapterAndroid,
            String address, Object bluetoothDeviceWrapper, List<ParcelUuid> advertisedUuids);
}
