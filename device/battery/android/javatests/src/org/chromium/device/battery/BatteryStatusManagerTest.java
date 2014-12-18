// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.battery;

import android.content.Intent;
import android.os.BatteryManager;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.mojom.device.BatteryStatus;

/**
 * Test suite for BatteryStatusManager.
 */
public class BatteryStatusManagerTest extends AndroidTestCase {
    // Values reported in the most recent callback from |mManager|.
    private boolean mCharging = false;
    private double mChargingTime = 0;
    private double mDischargingTime = 0;
    private double mLevel = 0;

    private BatteryStatusManager.BatteryStatusCallback mCallback =
            new BatteryStatusManager.BatteryStatusCallback() {
        @Override
        public void onBatteryStatusChanged(BatteryStatus batteryStatus) {
            mCharging = batteryStatus.charging;
            mChargingTime = batteryStatus.chargingTime;
            mDischargingTime = batteryStatus.dischargingTime;
            mLevel = batteryStatus.level;
        }
    };

    private BatteryStatusManager mManager;

    private void verifyValues(
            boolean charging, double chargingTime, double dischargingTime, double level) {
        assertEquals(charging, mCharging);
        assertEquals(chargingTime, mChargingTime);
        assertEquals(dischargingTime, mDischargingTime);
        assertEquals(level, mLevel);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mManager =
                BatteryStatusManager.createBatteryStatusManagerForTesting(getContext(), mCallback);
    }

    @SmallTest
    public void testOnReceiveBatteryNotPluggedIn() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, 0);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 10);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        mManager.onReceive(intent);
        verifyValues(false, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY, 0.1);
    }

    @SmallTest
    public void testOnReceiveBatteryPluggedInACCharging() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_AC);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_CHARGING);

        mManager.onReceive(intent);
        verifyValues(true, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY, 0.5);
    }

    @SmallTest
    public void testOnReceiveBatteryPluggedInACNotCharging() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_AC);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        mManager.onReceive(intent);
        verifyValues(true, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY, 0.5);
    }

    @SmallTest
    public void testOnReceiveBatteryPluggedInUSBFull() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_USB);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 100);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_FULL);

        mManager.onReceive(intent);
        verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @SmallTest
    public void testOnReceiveNoBattery() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, false);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_USB);

        mManager.onReceive(intent);
        verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @SmallTest
    public void testOnReceiveNoPluggedStatus() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);

        mManager.onReceive(intent);
        verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @SmallTest
    public void testStartStopSucceeds() {
        assertTrue(mManager.start());
        mManager.stop();
    }
}
