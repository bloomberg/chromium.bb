// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.offlinepages.DeviceConditions;
import org.chromium.chrome.browser.offlinepages.ShadowDeviceConditions;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link PrefetchBackgroundTask}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMultiDex.class, ShadowDeviceConditions.class})
public class PrefetchBackgroundTaskUnitTest {
    /**
     * Fake of BackgroundTaskScheduler system service.
     */
    public static class FakeBackgroundTaskScheduler implements BackgroundTaskScheduler {
        private HashMap<Integer, TaskInfo> mTaskInfos = new HashMap<>();

        @Override
        public boolean schedule(Context context, TaskInfo taskInfo) {
            mTaskInfos.put(taskInfo.getTaskId(), taskInfo);
            return true;
        }

        @Override
        public void cancel(Context context, int taskId) {
            mTaskInfos.remove(taskId);
        }

        @Override
        public void checkForOSUpgrade(Context context) {}

        @Override
        public void reschedule(Context context) {}

        public TaskInfo getTaskInfo(int taskId) {
            return mTaskInfos.get(taskId);
        }

        public void clear() {
            mTaskInfos = new HashMap<>();
        }
    }
    public static final boolean POWER_CONNECTED = true;
    public static final boolean POWER_SAVE_MODE_ON = true;
    public static final int HIGH_BATTERY_LEVEL = 75;
    public static final int LOW_BATTERY_LEVEL = 25;

    @Spy
    private PrefetchBackgroundTask mPrefetchBackgroundTask = new PrefetchBackgroundTask();
    @Mock
    private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Captor
    ArgumentCaptor<BrowserParts> mBrowserParts;
    private FakeBackgroundTaskScheduler mFakeTaskScheduler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doNothing().when(mChromeBrowserInitializer).handlePreNativeStartup(any(BrowserParts.class));
        try {
            doAnswer(new Answer<Void>() {
                @Override
                public Void answer(InvocationOnMock invocation) {
                    mBrowserParts.getValue().finishNativeInitialization();
                    return null;
                }
            })
                    .when(mChromeBrowserInitializer)
                    .handlePostNativeStartup(eq(true), mBrowserParts.capture());
        } catch (ProcessInitException ex) {
            fail("Unexpected exception while initializing mock of ChromeBrowserInitializer.");
        }

        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);

        doAnswer(new Answer() {
            public Object answer(InvocationOnMock invocation) {
                mPrefetchBackgroundTask.setNativeTask(1);
                return Boolean.TRUE;
            }
        })
                .when(mPrefetchBackgroundTask)
                .nativeStartPrefetchTask(any());
        doReturn(true).when(mPrefetchBackgroundTask).nativeOnStopTask(1);
        doReturn(null).when(mPrefetchBackgroundTask).getProfile();

        mFakeTaskScheduler = new FakeBackgroundTaskScheduler();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mFakeTaskScheduler);
    }

    @Test
    public void scheduleTask() {
        final int additionalDelaySeconds = 15;
        PrefetchBackgroundTaskScheduler.scheduleTask(additionalDelaySeconds);
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.SECONDS.toMillis(
                             PrefetchBackgroundTaskScheduler.DEFAULT_START_DELAY_SECONDS
                             + additionalDelaySeconds),
                scheduledTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(true, scheduledTask.isPersisted());
        assertEquals(TaskInfo.NETWORK_TYPE_UNMETERED, scheduledTask.getRequiredNetworkType());
    }

    @Test
    public void cancelTask() {
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNull(scheduledTask);

        PrefetchBackgroundTaskScheduler.scheduleTask(0);
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.SECONDS.toMillis(
                             PrefetchBackgroundTaskScheduler.DEFAULT_START_DELAY_SECONDS),
                scheduledTask.getOneOffInfo().getWindowStartTimeMs());

        PrefetchBackgroundTaskScheduler.cancelTask();
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNull(scheduledTask);
    }

    @Test
    public void createNativeTask() {
        final ArrayList<Boolean> reschedules = new ArrayList<>();
        TaskParameters params = mock(TaskParameters.class);
        when(params.getTaskId()).thenReturn(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);

        // Setup battery conditions with no power connected.
        DeviceConditions deviceConditions = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions, false /* metered */);

        mPrefetchBackgroundTask.onStartTask(null, params, new TaskFinishedCallback() {
            @Override
            public void taskFinished(boolean needsReschedule) {
                reschedules.add(needsReschedule);
            }
        });
        mPrefetchBackgroundTask.doneProcessing(false);

        assertEquals(1, reschedules.size());
        assertEquals(false, reschedules.get(0));
    }

    @Test
    public void testBatteryLow() throws Exception {
        // Setup low battery conditions with no power connected.
        DeviceConditions deviceConditionsLowBattery = new DeviceConditions(!POWER_CONNECTED,
                LOW_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsLowBattery, true /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, battery conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.RESCHEDULE, result);
    }

    @Test
    public void testBatteryHigh() throws Exception {
        // Setup high battery conditions with no power connected.
        DeviceConditions deviceConditionsHighBattery = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(
                deviceConditionsHighBattery, false /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        // Nothing to do.
                    }
                });
        assertEquals(NativeBackgroundTask.LOAD_NATIVE, result);
    }

    @Test
    public void testNoNetwork() throws Exception {
        // Setup high battery conditions with no power connected.
        DeviceConditions deviceConditionsHighBattery = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_NONE, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(
                deviceConditionsHighBattery, false /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.RESCHEDULE, result);
    }

    @Test
    public void testUnmeteredWifiNetwork() throws Exception {
        // Setup high battery conditions with no power connected.
        DeviceConditions deviceConditionsHighBattery = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(
                deviceConditionsHighBattery, false /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.LOAD_NATIVE, result);
    }

    @Test
    public void testMeteredWifiNetwork() throws Exception {
        // Setup high battery conditions with no power connected.
        DeviceConditions deviceConditionsHighBattery = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(
                deviceConditionsHighBattery, true /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.RESCHEDULE, result);
    }

    @Test
    public void testMetered2GNetwork() throws Exception {
        // Setup high battery conditions with no power connected.
        DeviceConditions deviceConditionsHighBattery = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_2G, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(
                deviceConditionsHighBattery, false /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.RESCHEDULE, result);
    }

    @Test
    public void testBluetoothNetwork() throws Exception {
        // Setup high battery conditions with no power connected.
        DeviceConditions deviceConditionsHighBattery = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_BLUETOOTH, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(
                deviceConditionsHighBattery, false /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.RESCHEDULE, result);
    }

    @Test
    public void testOnStopAfterCallback() throws Exception {
        final ArrayList<Boolean> reschedules = new ArrayList<>();
        TaskParameters params = mock(TaskParameters.class);
        when(params.getTaskId()).thenReturn(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);

        // Conditions should be appropriate for running the task.
        DeviceConditions deviceConditions = new DeviceConditions(POWER_CONNECTED,
                HIGH_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions, false /* metered */);

        mPrefetchBackgroundTask.onStartTask(null, params, new TaskFinishedCallback() {
            @Override
            public void taskFinished(boolean needsReschedule) {
                reschedules.add(needsReschedule);
            }
        });
        mPrefetchBackgroundTask.doneProcessing(false);
        mPrefetchBackgroundTask.onStopTaskWithNative(RuntimeEnvironment.application, params);

        assertEquals(1, reschedules.size());
        assertEquals(false, reschedules.get(0));
    }

    @Test
    public void testPowerSaverOn() throws Exception {
        // Setup power save mode, battery is high, wifi, not plugged in.
        DeviceConditions deviceConditionsPowerSave = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, POWER_SAVE_MODE_ON);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsPowerSave, false /* metered */);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, battery conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.RESCHEDULE, result);
    }
}
