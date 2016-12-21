// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.precache;

import android.content.Context;
import android.net.ConnectivityManager;
import android.os.BatteryManager;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;

/**
 * Tests for {@link DeviceState}.
 */
public class DeviceStateTest extends InstrumentationTestCase {

    /**
     * Factory to create {@link MockNetworkInfoDelegate} instances.
     */
    static class MockNetworkInfoDelegateFactory extends NetworkInfoDelegateFactory {
        private final boolean mIsValid;
        private final int mType;
        private final boolean mIsAvailable;
        private final boolean mIsConnected;
        private final boolean mIsRoaming;
        private final boolean mIsActiveNetworkMetered;

        MockNetworkInfoDelegateFactory(boolean valid, int type, boolean available,
                boolean connected, boolean roaming, boolean activeNetworkMetered) {
            mIsValid = valid;
            mType = type;
            mIsAvailable = available;
            mIsConnected = connected;
            mIsRoaming = roaming;
            mIsActiveNetworkMetered = activeNetworkMetered;
        }

        @Override
        NetworkInfoDelegate getNetworkInfoDelegate(Context context) {
            return new MockNetworkInfoDelegate(mIsValid, mType, mIsAvailable, mIsConnected,
                    mIsRoaming, mIsActiveNetworkMetered);
        }
    }

    /**
     * Mock of {@link NetworkInfoDelegate}.
     */
    static class MockNetworkInfoDelegate extends NetworkInfoDelegate {
        private final boolean mIsValid;
        private final int mType;
        private final boolean mIsAvailable;
        private final boolean mIsConnected;
        private final boolean mIsRoaming;
        private final boolean mIsActiveNetworkMetered;

        MockNetworkInfoDelegate(boolean valid, int type, boolean available, boolean connected,
                boolean roaming, boolean activeNetworkMetered) {
            mIsValid = valid;
            mType = type;
            mIsAvailable = available;
            mIsConnected = connected;
            mIsRoaming = roaming;
            mIsActiveNetworkMetered = activeNetworkMetered;
        }

        @Override
        protected void getNetworkInfo(Context context) {}

        @Override
        protected boolean isValid() {
            return mIsValid;
        }

        @Override
        protected int getType() {
            return mType;
        }

        @Override
        protected boolean isAvailable() {
            return mIsAvailable;
        }

        @Override
        protected boolean isConnected() {
            return mIsConnected;
        }

        @Override
        protected boolean isRoaming() {
            return mIsRoaming;
        }

        @Override
        protected boolean isActiveNetworkMetered() {
            return mIsActiveNetworkMetered;
        }
    }

    /**
     * Mock of {@link DeviceState}.
     */
    static class MockDeviceState extends DeviceState {
        int mBatteryStatus;

        @Override
        int getStickyBatteryStatus(Context context) {
            return mBatteryStatus;
        }

        public void setStickyBatteryStatus(int status) {
            mBatteryStatus = status;
        }
    }

    @SmallTest
    @Feature({"Precache"})
    public void testPowerConnected() {
        AdvancedMockContext context = new AdvancedMockContext();
        MockDeviceState deviceState = new MockDeviceState();

        deviceState.setStickyBatteryStatus(BatteryManager.BATTERY_STATUS_NOT_CHARGING);
        assertFalse(deviceState.isPowerConnected(context));

        deviceState.setStickyBatteryStatus(BatteryManager.BATTERY_STATUS_CHARGING);
        assertTrue(deviceState.isPowerConnected(context));

        deviceState.setStickyBatteryStatus(BatteryManager.BATTERY_STATUS_FULL);
        assertTrue(deviceState.isPowerConnected(context));
    }

    @SmallTest
    @Feature({"Precache"})
    public void testUnmeteredNetworkAvailable() {
        AdvancedMockContext context = new AdvancedMockContext();
        DeviceState deviceState = DeviceState.getInstance();

        // Expect WiFi to be reported as available because there is valid {@link NetworkInfo},
        // the connection is WiFi, it's available and connected, and not roaming or metered.
        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(
                        true, ConnectivityManager.TYPE_WIFI, true, true, false, false));
        assertTrue(deviceState.isUnmeteredNetworkAvailable(context));

        // Expect WiFi to be reported as unavailable because one of the aforementioned required
        // conditions is not met.
        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(
                        false, ConnectivityManager.TYPE_WIFI, true, true, false, false));
        assertFalse(deviceState.isUnmeteredNetworkAvailable(context));

        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(true, 0, false, true, false, false));
        assertFalse(deviceState.isUnmeteredNetworkAvailable(context));

        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(
                        true, ConnectivityManager.TYPE_WIFI, false, true, false, false));
        assertFalse(deviceState.isUnmeteredNetworkAvailable(context));

        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(
                        true, ConnectivityManager.TYPE_WIFI, true, false, false, false));
        assertFalse(deviceState.isUnmeteredNetworkAvailable(context));

        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(
                        true, ConnectivityManager.TYPE_WIFI, true, true, true, false));
        assertFalse(deviceState.isUnmeteredNetworkAvailable(context));

        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(
                        true, ConnectivityManager.TYPE_WIFI, true, true, false, true));
        assertFalse(deviceState.isUnmeteredNetworkAvailable(context));

        deviceState.setNetworkInfoDelegateFactory(
                new MockNetworkInfoDelegateFactory(true, 0, false, false, true, true));
        assertFalse(deviceState.isUnmeteredNetworkAvailable(context));
    }
}
