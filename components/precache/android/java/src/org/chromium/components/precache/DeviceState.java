// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.precache;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.os.BatteryManager;

import org.chromium.base.VisibleForTesting;

/**
 * Utility class that provides information about the current state of the device.
 */
public class DeviceState {
    private static DeviceState sDeviceState = null;

    /** Disallow Construction of DeviceState objects. Use {@link #getInstance()} instead to create
     * a singleton instance.
     */
    protected DeviceState() {}

    public static DeviceState getInstance() {
        if (sDeviceState == null) sDeviceState = new DeviceState();
        return sDeviceState;
    }

    protected NetworkInfoDelegateFactory mNetworkInfoDelegateFactory =
            new NetworkInfoDelegateFactory();

    @VisibleForTesting
    void setNetworkInfoDelegateFactory(NetworkInfoDelegateFactory factory) {
        mNetworkInfoDelegateFactory = factory;
    }

    /** @return integer representing the current status of the battery. */
    @VisibleForTesting
    int getStickyBatteryStatus(Context context) {
        IntentFilter iFilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        // Call registerReceiver on context.getApplicationContext(), not on context itself, because
        // context could be a BroadcastReceiver context, which would throw an
        // android.content.ReceiverCallNotAllowedException.
        Intent batteryStatus = context.getApplicationContext().registerReceiver(null, iFilter);

        if (batteryStatus == null) {
            return BatteryManager.BATTERY_STATUS_UNKNOWN;
        }
        return batteryStatus.getIntExtra(BatteryManager.EXTRA_STATUS,
                BatteryManager.BATTERY_STATUS_UNKNOWN);
    }

    /** @return whether the device is connected to power. */
    public boolean isPowerConnected(Context context) {
        int status = getStickyBatteryStatus(context);
        return status == BatteryManager.BATTERY_STATUS_CHARGING
                || status == BatteryManager.BATTERY_STATUS_FULL;
    }

    /** @return whether the currently active network is Wi-Fi, not roaming, and not metered. */
    public boolean isWifiAvailable(Context context) {
        NetworkInfoDelegate networkInfo =
                mNetworkInfoDelegateFactory.getNetworkInfoDelegate(context);
        return (networkInfo.isValid()
                && networkInfo.getType() == ConnectivityManager.TYPE_WIFI
                && networkInfo.isAvailable()
                && networkInfo.isConnected()
                && !networkInfo.isRoaming()
                && !networkInfo.isActiveNetworkMetered());
    }
}

