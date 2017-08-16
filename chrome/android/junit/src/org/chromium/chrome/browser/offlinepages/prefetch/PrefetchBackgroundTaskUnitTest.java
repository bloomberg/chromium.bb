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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link PrefetchBackgroundTask}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMultiDex.class})
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
        PrefetchBackgroundTask.scheduleTask(additionalDelaySeconds, true);
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.SECONDS.toMillis(PrefetchBackgroundTask.DEFAULT_START_DELAY_SECONDS
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

        PrefetchBackgroundTask.scheduleTask(0, true);
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.SECONDS.toMillis(PrefetchBackgroundTask.DEFAULT_START_DELAY_SECONDS),
                scheduledTask.getOneOffInfo().getWindowStartTimeMs());

        PrefetchBackgroundTask.cancelTask();
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNull(scheduledTask);
    }

    @Test
    public void createNativeTask() {
        final ArrayList<Boolean> reschedules = new ArrayList<>();
        TaskParameters params = mock(TaskParameters.class);
        when(params.getTaskId()).thenReturn(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);

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
}
