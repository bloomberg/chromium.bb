// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskScheduler}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskSchedulerTest {
    private static final TaskInfo TASK =
            TaskInfo.createOneOffTask(
                            TaskIds.TEST, TestBackgroundTask.class, TimeUnit.DAYS.toMillis(1))
                    .build();

    @Mock
    private BackgroundTaskSchedulerDelegate mDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(
                new BackgroundTaskScheduler(mDelegate));
        TestBackgroundTask.reset();
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskSuccessful() {
        doReturn(true).when(mDelegate).schedule(eq(RuntimeEnvironment.application), eq(TASK));
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                RuntimeEnvironment.application, TASK);
        assertTrue(BackgroundTaskSchedulerPrefs.getScheduledTasks().contains(
                TASK.getBackgroundTaskClass().getName()));
        verify(mDelegate, times(1)).schedule(eq(RuntimeEnvironment.application), eq(TASK));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskFailed() {
        doReturn(false).when(mDelegate).schedule(eq(RuntimeEnvironment.application), eq(TASK));
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                RuntimeEnvironment.application, TASK);
        assertFalse(BackgroundTaskSchedulerPrefs.getScheduledTasks().contains(
                TASK.getBackgroundTaskClass().getName()));
        verify(mDelegate, times(1)).schedule(eq(RuntimeEnvironment.application), eq(TASK));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancel() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK);

        doNothing().when(mDelegate).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                RuntimeEnvironment.application, TaskIds.TEST);
        assertFalse(BackgroundTaskSchedulerPrefs.getScheduledTasks().contains(
                TASK.getBackgroundTaskClass().getName()));
        verify(mDelegate, times(1)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testRescheduleTasks() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK);

        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
        assertFalse(BackgroundTaskSchedulerPrefs.getScheduledTasks().isEmpty());
        BackgroundTaskSchedulerFactory.getScheduler().reschedule(RuntimeEnvironment.application);

        assertEquals(1, TestBackgroundTask.getRescheduleCalls());
        assertTrue(BackgroundTaskSchedulerPrefs.getScheduledTasks().isEmpty());
    }
}
