// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.os.Build;
import android.os.ParcelUuid;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;

import java.util.ArrayList;
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

        /**
         * Creates and discovers a new device.
         */
        @CalledByNative("FakeBluetoothAdapter")
        public void discoverLowEnergyDevice(int deviceOrdinal) {
            switch (deviceOrdinal) {
                case 1: {
                    ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                    uuids.add(ParcelUuid.fromString("00001800-0000-1000-8000-00805f9b34fb"));
                    uuids.add(ParcelUuid.fromString("00001801-0000-1000-8000-00805f9b34fb"));

                    mFakeScanner.mCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice(
                                                       "AA:00:00:00:00:01", "FakeBluetoothDevice"),
                                                                uuids));
                    break;
                }
                case 2: {
                    ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                    uuids.add(ParcelUuid.fromString("00001802-0000-1000-8000-00805f9b34fb"));
                    uuids.add(ParcelUuid.fromString("00001803-0000-1000-8000-00805f9b34fb"));

                    mFakeScanner.mCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice(
                                                       "AA:00:00:00:00:01", "FakeBluetoothDevice"),
                                                                uuids));
                    break;
                }
                case 3: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice("AA:00:00:00:00:01", ""),
                                                                uuids));

                    break;
                }
                case 4: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice("BB:00:00:00:00:02", ""),
                                                                uuids));

                    break;
                }
            }
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
        private final ArrayList<ParcelUuid> mUuids;

        FakeScanResult(FakeBluetoothDevice device, ArrayList<ParcelUuid> uuids) {
            super(null);
            mDevice = device;
            mUuids = uuids;
        }

        @Override
        public Wrappers.BluetoothDeviceWrapper getDevice() {
            return mDevice;
        }

        @Override
        public List<ParcelUuid> getScanRecord_getServiceUuids() {
            return mUuids;
        }
    }

    /**
     * Fakes android.bluetooth.BluetoothDevice.
     */
    static class FakeBluetoothDevice extends Wrappers.BluetoothDeviceWrapper {
        private String mAddress;
        private String mName;

        public FakeBluetoothDevice(String address, String name) {
            super(null);
            mAddress = address;
            mName = name;
        }

        @Override
        public String getAddress() {
            return mAddress;
        }

        @Override
        public int getBluetoothClass_getDeviceClass() {
            return 0x1F00; // Unspecified Device Class
        }

        @Override
        public int getBondState() {
            return BluetoothDevice.BOND_BONDED;
        }

        @Override
        public String getName() {
            return mName;
        }
    }
}
