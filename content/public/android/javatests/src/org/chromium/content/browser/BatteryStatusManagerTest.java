// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.content.Intent;

import android.os.BatteryManager;

import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 * Test suite for BatteryStatusManager.
 */
public class BatteryStatusManagerTest extends AndroidTestCase {

    private BatteryStatusManagerForTests mBatteryStatusManager;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mBatteryStatusManager = BatteryStatusManagerForTests.getInstance(getContext());
    }

    @SmallTest
    public void testOnReceiveBatteryNotPluggedIn() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, 0);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 10);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        mBatteryStatusManager.onReceive(intent);

        mBatteryStatusManager.verifyCalls("gotBatteryStatus");
        mBatteryStatusManager.verifyValues(false, Double.POSITIVE_INFINITY,
                Double.POSITIVE_INFINITY, 0.1);
    }

    @SmallTest
    public void testOnReceiveBatteryPluggedInACCharging() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_AC);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_CHARGING);

        mBatteryStatusManager.onReceive(intent);

        mBatteryStatusManager.verifyCalls("gotBatteryStatus");
        mBatteryStatusManager.verifyValues(true, Double.POSITIVE_INFINITY,
                Double.POSITIVE_INFINITY, 0.5);
    }

    @SmallTest
    public void testOnReceiveBatteryPluggedInACNotCharging() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_AC);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        mBatteryStatusManager.onReceive(intent);

        mBatteryStatusManager.verifyCalls("gotBatteryStatus");
        mBatteryStatusManager.verifyValues(true, Double.POSITIVE_INFINITY,
                Double.POSITIVE_INFINITY, 0.5);
    }

    @SmallTest
    public void testOnReceiveBatteryPluggedInUSBFull() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_USB);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 100);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_FULL);

        mBatteryStatusManager.onReceive(intent);

        mBatteryStatusManager.verifyCalls("gotBatteryStatus");
        mBatteryStatusManager.verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @SmallTest
    public void testOnReceiveNoBattery() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, false);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_USB);

        mBatteryStatusManager.onReceive(intent);

        mBatteryStatusManager.verifyCalls("gotBatteryStatus");
        mBatteryStatusManager.verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @SmallTest
    public void testOnReceiveNoPluggedStatus() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);

        mBatteryStatusManager.onReceive(intent);

        mBatteryStatusManager.verifyCalls("gotBatteryStatus");
        mBatteryStatusManager.verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @SmallTest
    public void testStartStopSucceeds() {
        assertTrue(mBatteryStatusManager.start(0));
        mBatteryStatusManager.stop();
    }

    // Helper class for testing.

    private static class BatteryStatusManagerForTests extends BatteryStatusManager {

        private boolean mCharging = false;
        private double mChargingTime = 0;
        private double mDischargingTime = 0;
        private double mLevel = 0;
        private String mCalls = "";

        private BatteryStatusManagerForTests(Context context) {
            super(context);
        }

        static BatteryStatusManagerForTests getInstance(Context context) {
            return new BatteryStatusManagerForTests(context);
        }

        private void verifyValues(boolean charging, double chargingTime,
                double dischargingTime, double level) {
            assertEquals(charging, mCharging);
            assertEquals(chargingTime, mChargingTime);
            assertEquals(dischargingTime, mDischargingTime);
            assertEquals(level, mLevel);
        }

        private void verifyCalls(String names) {
            assertEquals(mCalls, names);
        }

        @Override
        protected boolean ignoreBatteryPresentState() {
            return false;
        }

        @Override
        protected void gotBatteryStatus(boolean charging, double chargingTime,
                double dischargingTime, double level) {
            mCharging = charging;
            mChargingTime = chargingTime;
            mDischargingTime = dischargingTime;
            mLevel = level;
            mCalls = mCalls.concat("gotBatteryStatus");
        }
    }

}
