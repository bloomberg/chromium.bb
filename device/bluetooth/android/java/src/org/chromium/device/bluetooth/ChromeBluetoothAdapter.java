// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.ScanSettings;
import android.os.Build;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.Log;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothAdapter as necessary for C++
 * device::BluetoothAdapterAndroid, which implements the cross platform
 * device::BluetoothAdapter.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
final class ChromeBluetoothAdapter {
    private static final String TAG = "cr.Bluetooth";

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
    // 'Object' type must be used because inner class Wrappers.BluetoothAdapterWrapper reference is
    // not handled by jni_generator.py JavaToJni. http://crbug.com/505554
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
     * Starts a Low Energy scan.
     * @return True on success.
     */
    private boolean startScan() {
        // scanMode note: SCAN_FAILED_FEATURE_UNSUPPORTED is caused (at least on some devices) if
        // setReportDelay() is used or if SCAN_MODE_LOW_LATENCY isn't used.
        int scanMode = ScanSettings.SCAN_MODE_LOW_LATENCY;

        assert mScanCallback == null;
        mScanCallback = new ScanCallback();

        try {
            mAdapter.getBluetoothLeScanner().startScan(null /* filters */, scanMode, mScanCallback);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot start scan: " + e);
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
            mAdapter.getBluetoothLeScanner().stopScan(mScanCallback);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot stop scan: " + e);
            mScanCallback = null;
            return false;
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
}
