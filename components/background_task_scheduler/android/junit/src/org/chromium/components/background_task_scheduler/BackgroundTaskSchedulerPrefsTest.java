// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskSchedulerPrefs}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskSchedulerPrefsTest {
    private static final TaskInfo TASK_1 =
            TaskInfo.createOneOffTask(
                            TaskIds.TEST, TestBackgroundTask.class, TimeUnit.DAYS.toMillis(1))
                    .build();
    private static final TaskInfo TASK_2 =
            TaskInfo.createOneOffTask(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID,
                            TestBackgroundTask2.class, TimeUnit.DAYS.toMillis(1))
                    .build();

    /**
     * Dummy extension of the class above to ensure scheduled tasks returned from shared
     * preferences are not missing something.
     */
    static class TestBackgroundTask2 extends TestBackgroundTask {}

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testAddScheduledTask() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK_1);
        assertEquals("We are expecting a single entry.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTasks().size());

        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK_1);
        assertEquals("Still there should be only one entry, as duplicate was added.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTasks().size());

        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK_2);
        assertEquals("There should be 2 tasks in shared prefs.", 2,
                BackgroundTaskSchedulerPrefs.getScheduledTasks().size());
        TaskInfo task3 = TaskInfo.createOneOffTask(TaskIds.OMAHA_JOB_ID, TestBackgroundTask2.class,
                                         TimeUnit.DAYS.toMillis(1))
                                 .build();

        BackgroundTaskSchedulerPrefs.addScheduledTask(task3);
        // Still expecting only 2 classes, because of shared class between 2 tasks.
        assertEquals("There should be 2 tasks in shared prefs.", 2,
                BackgroundTaskSchedulerPrefs.getScheduledTasks().size());

        Set<String> scheduledTasks = BackgroundTaskSchedulerPrefs.getScheduledTasks();
        assertTrue("TASK_1 class name in scheduled tasks.",
                scheduledTasks.contains(TASK_1.getBackgroundTaskClass().getName()));
        assertTrue("TASK_2 class name in scheduled tasks.",
                scheduledTasks.contains(TASK_2.getBackgroundTaskClass().getName()));
        assertTrue("task3 class name in scheduled tasks.",
                scheduledTasks.contains(task3.getBackgroundTaskClass().getName()));

        Set<Integer> taskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
        assertTrue(taskIds.contains(TASK_1.getTaskId()));
        assertTrue(taskIds.contains(TASK_2.getTaskId()));
        assertTrue(taskIds.contains(task3.getTaskId()));
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testRemoveScheduledTask() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK_1);
        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK_2);
        BackgroundTaskSchedulerPrefs.removeScheduledTask(TASK_1.getTaskId());
        assertEquals("We are expecting a single entry.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTasks().size());

        BackgroundTaskSchedulerPrefs.removeScheduledTask(TASK_1.getTaskId());
        assertEquals("Removing a task which is not there does not affect the task set.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTasks().size());

        Set<String> scheduledTasks = BackgroundTaskSchedulerPrefs.getScheduledTasks();
        assertFalse("TASK_1 class name is not in scheduled tasks.",
                scheduledTasks.contains(TASK_1.getBackgroundTaskClass().getName()));
        assertTrue("TASK_2 class name in scheduled tasks.",
                scheduledTasks.contains(TASK_2.getBackgroundTaskClass().getName()));

        Set<Integer> taskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
        assertFalse(taskIds.contains(TASK_1.getTaskId()));
        assertTrue(taskIds.contains(TASK_2.getTaskId()));
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testUnparseableEntries() {
        HashSet<String> badEntries = new HashSet<>();
        badEntries.add(":123");
        badEntries.add("Class:");
        badEntries.add("Class:NotAnInt");
        badEntries.add("Int field missing");
        badEntries.add("Class:123:Too many fields");
        badEntries.add("");
        badEntries.add(null);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putStringSet("bts_scheduled_tasks", badEntries)
                .apply();
        assertTrue(BackgroundTaskSchedulerPrefs.getScheduledTaskIds().isEmpty());
        assertTrue(BackgroundTaskSchedulerPrefs.getScheduledTasks().isEmpty());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testRemoveAllTasks() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK_1);
        BackgroundTaskSchedulerPrefs.addScheduledTask(TASK_2);
        BackgroundTaskSchedulerPrefs.removeAllTasks();
        assertTrue("We are expecting a all tasks to be gone.",
                BackgroundTaskSchedulerPrefs.getScheduledTasks().isEmpty());
        assertTrue("We are expecting a all tasks to be gone.",
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().isEmpty());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testLastSdkVersion() {
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.KITKAT);
        assertEquals("Current SDK version should be default.", Build.VERSION_CODES.KITKAT,
                BackgroundTaskSchedulerPrefs.getLastSdkVersion());
        BackgroundTaskSchedulerPrefs.setLastSdkVersion(Build.VERSION_CODES.LOLLIPOP);
        assertEquals(
                Build.VERSION_CODES.LOLLIPOP, BackgroundTaskSchedulerPrefs.getLastSdkVersion());
    }
}
