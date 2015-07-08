// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.le.ScanFilter;
import android.os.Build;

import org.chromium.base.CalledByNative;
import org.chromium.base.Log;

import java.util.List;

/**
 * Fake implementations of android.bluetooth.* classes for testing.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
class Fakes {
    private static final String TAG = "cr.Bluetooth";

    /**
     * Fakes android.bluetooth.BluetoothAdapter.
     */
    static class FakeBluetoothAdapter extends Wrappers.BluetoothAdapterWrapper {
        private final FakeBluetoothLeScanner mFakeScanner;

        /**
         * Creates a FakeBluetoothAdapter.
         */
        @CalledByNative("FakeBluetoothAdapter")
        public static FakeBluetoothAdapter create() {
            Log.v(TAG, "FakeBluetoothAdapter created.");
            return new FakeBluetoothAdapter();
        }

        private FakeBluetoothAdapter() {
            super(null, new FakeBluetoothLeScanner());
            mFakeScanner = (FakeBluetoothLeScanner) mScanner;
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothAdapterWrapper overrides:

        @Override
        public boolean isEnabled() {
            return true;
        }

        @Override
        public String getAddress() {
            return "A1:B2:C3:D4:E5:F6";
        }

        @Override
        public String getName() {
            return "FakeBluetoothAdapter";
        }

        @Override
        public int getScanMode() {
            return android.bluetooth.BluetoothAdapter.SCAN_MODE_NONE;
        }

        @Override
        public boolean isDiscovering() {
            return false;
        }
    }

    /**
     * Fakes android.bluetooth.le.BluetoothLeScanner.
     */
    static class FakeBluetoothLeScanner extends Wrappers.BluetoothLeScannerWrapper {
        public Wrappers.ScanCallbackWrapper mCallback;

        private FakeBluetoothLeScanner() {
            super(null);
        }

        @Override
        public void startScan(List<ScanFilter> filters, int scanSettingsScanMode,
                Wrappers.ScanCallbackWrapper callback) {
            if (mCallback != null) {
                throw new IllegalArgumentException(
                        "FakeBluetoothLeScanner does not support multiple scans.");
            }
            mCallback = callback;
        }

        @Override
        public void stopScan(Wrappers.ScanCallbackWrapper callback) {
            if (mCallback != callback) {
                throw new IllegalArgumentException("No scan in progress.");
            }
            mCallback = null;
        }
    }

    /**
     * Fakes android.bluetooth.le.ScanResult
     */
    static class FakeScanResult extends Wrappers.ScanResultWrapper {
        private final FakeBluetoothDevice mDevice;

        FakeScanResult(FakeBluetoothDevice device) {
            super(null);
            mDevice = device;
        }

        @Override
        public Wrappers.BluetoothDeviceWrapper getDevice() {
            return mDevice;
        }
    }

    /**
     * Fakes android.bluetooth.BluetoothDevice.
     */
    static class FakeBluetoothDevice extends Wrappers.BluetoothDeviceWrapper {
        private static final String ADDRESS = "A1:B2:C3:DD:DD:DD";
        private static final String NAME = "FakeBluetoothDevice";

        public FakeBluetoothDevice() {
            super(null);
        }

        @Override
        public String getAddress() {
            return ADDRESS;
        }

        @Override
        public String getName() {
            return NAME;
        }
    }
}
