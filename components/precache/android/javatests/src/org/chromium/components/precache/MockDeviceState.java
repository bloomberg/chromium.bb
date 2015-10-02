// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.precache;

import android.content.Context;

/**
 * Mock of the DeviceState class.
 */
public class MockDeviceState extends DeviceState {
    private final int mStickyBatteryStatus;
    private final boolean mPowerIsConnected;
    private final boolean mWifiIsAvailable;

    public MockDeviceState(
            int stickyBatteryStatus, boolean powerIsConnected, boolean wifiIsAvailable) {
        mStickyBatteryStatus = stickyBatteryStatus;
        mPowerIsConnected = powerIsConnected;
        mWifiIsAvailable = wifiIsAvailable;
    }

    @Override
    int getStickyBatteryStatus(Context context) {
        return mStickyBatteryStatus;
    }

    @Override
    public boolean isPowerConnected(Context context) {
        return mPowerIsConnected;
    }

    @Override
    public boolean isWifiAvailable(Context context) {
        return mWifiIsAvailable;
    }
}
