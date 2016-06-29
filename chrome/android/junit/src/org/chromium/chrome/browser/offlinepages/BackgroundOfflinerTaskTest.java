// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Bundle;

import com.google.android.gms.gcm.Task;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeBackgroundServiceWaiter;
import org.chromium.net.ConnectionType;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Unit tests for BackgroundOfflinerTask.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        application = BaseChromiumApplication.class,
        shadows = { ShadowGcmNetworkManager.class })
public class BackgroundOfflinerTaskTest {
    private static final boolean REQUIRE_POWER = true;
    private static final boolean REQUIRE_UNMETERED = true;
    private static final boolean POWER_CONNECTED = true;
    private static final int MINIMUM_BATTERY_LEVEL = 33;

    @Mock
    private OfflinePageUtils mOfflinePageUtils;

    private Bundle mTaskExtras;
    private long mTestTime;
    private StubBackgroundSchedulerProcessor mStubBackgroundSchedulerProcessor;
    private TriggerConditions mTriggerConditions =
            new TriggerConditions(!REQUIRE_POWER, MINIMUM_BATTERY_LEVEL, REQUIRE_UNMETERED);
    private DeviceConditions mDeviceConditions = new DeviceConditions(
            !POWER_CONNECTED, MINIMUM_BATTERY_LEVEL + 5, ConnectionType.CONNECTION_3G);

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        when(mOfflinePageUtils.getDeviceConditionsImpl(any(Context.class)))
                .thenReturn(mDeviceConditions);

        // Build a bundle with trigger conditions.
        mTaskExtras = new Bundle();
        TaskExtrasPacker.packTimeInBundle(mTaskExtras);
        TaskExtrasPacker.packTriggerConditionsInBundle(mTaskExtras, mTriggerConditions);

        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        mStubBackgroundSchedulerProcessor = new StubBackgroundSchedulerProcessor();
        RecordHistogram.disableForTests();
        ShadowGcmNetworkManager.clear();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testIncomingTask() {
        BackgroundOfflinerTask task =
                new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1);
        task.processBackgroundRequests(mTaskExtras, mDeviceConditions, waiter);

        // Check with ShadowBackgroundBackgroundSchedulerProcessor that startProcessing got called.
        assertTrue(mStubBackgroundSchedulerProcessor.getStartProcessingCalled());
        assertSame(mDeviceConditions, mStubBackgroundSchedulerProcessor.getDeviceConditions());

        // TODO(dougarnett): Call processor callback and verify waiter signaled.
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartBackgroundRequests() {
        BackgroundOfflinerTask task = new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1);
        assertNull("Nothing scheduled", ShadowGcmNetworkManager.getScheduledTask());
        assertTrue(task.startBackgroundRequests(Robolectric.application, mTaskExtras, waiter));

        // Check that the backup task was scheduled.
        Task gcmTask = ShadowGcmNetworkManager.getScheduledTask();
        assertNotNull("Backup task scheduled", gcmTask);
        assertEquals(mTriggerConditions,
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(gcmTask.getExtras()));

        // Check with ShadowBackgroundBackgroundSchedulerProcessor that startProcessing got called.
        assertTrue(mStubBackgroundSchedulerProcessor.getStartProcessingCalled());
        assertSame(mDeviceConditions, mStubBackgroundSchedulerProcessor.getDeviceConditions());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartBackgroundRequestsForLowBatteryLevel() {
        DeviceConditions deviceConditionsLowBattery = new DeviceConditions(
                !POWER_CONNECTED, MINIMUM_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI);
        when(mOfflinePageUtils.getDeviceConditionsImpl(any(Context.class)))
                .thenReturn(deviceConditionsLowBattery);
        BackgroundOfflinerTask task = new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter = new ChromeBackgroundServiceWaiter(1);
        assertNull("Nothing scheduled", ShadowGcmNetworkManager.getScheduledTask());
        assertFalse(task.startBackgroundRequests(Robolectric.application, mTaskExtras, waiter));

        // Check that the backup task was scheduled.
        Task gcmTask = ShadowGcmNetworkManager.getScheduledTask();
        assertNotNull("Backup task scheduled", gcmTask);
        assertEquals(mTriggerConditions,
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(gcmTask.getExtras()));

        // Check that startProcessing was NOT called.
        assertFalse(mStubBackgroundSchedulerProcessor.getStartProcessingCalled());

        // Now verify low battery level but with power connected will start processing.
        DeviceConditions deviceConditionsPowerConnected = new DeviceConditions(
                POWER_CONNECTED, MINIMUM_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI);
        when(mOfflinePageUtils.getDeviceConditionsImpl(any(Context.class)))
                .thenReturn(deviceConditionsPowerConnected);
        BackgroundOfflinerTask task2 =
                new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        ChromeBackgroundServiceWaiter waiter2 = new ChromeBackgroundServiceWaiter(1);
        assertTrue(task2.startBackgroundRequests(Robolectric.application, mTaskExtras, waiter2));
    }
}
