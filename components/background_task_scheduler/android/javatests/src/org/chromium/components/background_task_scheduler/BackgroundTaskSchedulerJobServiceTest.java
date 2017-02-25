// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link BackgroundTaskSchedulerJobService}.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
public class BackgroundTaskSchedulerJobServiceTest extends InstrumentationTestCase {
    private static class TestBackgroundTask implements BackgroundTask {
        @Override
        public boolean onStartTask(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            return false;
        }

        @Override
        public boolean onStopTask(Context context, TaskParameters taskParameters) {
            return false;
        }
    }

    @SmallTest
    public void testOneOffTaskInfoWithDeadlineConversion() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TimeUnit.MINUTES.toMillis(200))
                                      .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(), oneOffTask);
        assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        assertFalse(jobInfo.isPeriodic());
        assertEquals(oneOffTask.getOneOffInfo().getWindowEndTimeMs(),
                jobInfo.getMaxExecutionDelayMillis());
    }

    @SmallTest
    public void testOneOffTaskInfoWithWindowConversion() {
        TaskInfo oneOffTask =
                TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                TimeUnit.MINUTES.toMillis(100), TimeUnit.MINUTES.toMillis(200))
                        .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(), oneOffTask);
        assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        assertFalse(jobInfo.isPeriodic());
        assertEquals(
                oneOffTask.getOneOffInfo().getWindowStartTimeMs(), jobInfo.getMinLatencyMillis());
        assertEquals(oneOffTask.getOneOffInfo().getWindowEndTimeMs(),
                jobInfo.getMaxExecutionDelayMillis());
    }

    @SmallTest
    public void testPeriodicTaskInfoWithoutFlexConversion() {
        TaskInfo periodicTask = TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class,
                                                TimeUnit.MINUTES.toMillis(200))
                                        .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(), periodicTask);
        assertEquals(periodicTask.getTaskId(), jobInfo.getId());
        assertTrue(jobInfo.isPeriodic());
        assertEquals(periodicTask.getPeriodicInfo().getIntervalMs(), jobInfo.getIntervalMillis());
    }

    @SmallTest
    public void testPeriodicTaskInfoWithFlexConversion() {
        TaskInfo periodicTask =
                TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class,
                                TimeUnit.MINUTES.toMillis(200), TimeUnit.MINUTES.toMillis(50))
                        .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(), periodicTask);
        assertEquals(periodicTask.getTaskId(), jobInfo.getId());
        assertTrue(jobInfo.isPeriodic());
        assertEquals(periodicTask.getPeriodicInfo().getIntervalMs(), jobInfo.getIntervalMillis());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            assertEquals(periodicTask.getPeriodicInfo().getFlexMs(), jobInfo.getFlexMillis());
        }
    }

    @SmallTest
    public void testTaskInfoWithExtras() {
        Bundle taskExtras = new Bundle();
        taskExtras.putString("foo", "bar");
        taskExtras.putBoolean("bools", true);
        taskExtras.putLong("longs", 1342543L);
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TimeUnit.MINUTES.toMillis(200))
                                      .setExtras(taskExtras)
                                      .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(), oneOffTask);
        assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        PersistableBundle jobExtras = jobInfo.getExtras();
        PersistableBundle persistableBundle = jobExtras.getPersistableBundle(
                BackgroundTaskSchedulerJobService.BACKGROUND_TASK_EXTRAS_KEY);
        assertEquals(taskExtras.keySet().size(), persistableBundle.keySet().size());
        assertEquals(taskExtras.getString("foo"), persistableBundle.getString("foo"));
        assertEquals(taskExtras.getBoolean("bools"), persistableBundle.getBoolean("bools"));
        assertEquals(taskExtras.getLong("longs"), persistableBundle.getLong("longs"));
    }

    @SmallTest
    public void testTaskInfoWithManyConstraints() {
        TaskInfo.Builder taskBuilder = TaskInfo.createOneOffTask(
                TaskIds.TEST, TestBackgroundTask.class, TimeUnit.MINUTES.toMillis(200));

        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(), taskBuilder.setIsPersisted(true).build());
        assertTrue(jobInfo.isPersisted());

        jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(),
                taskBuilder.setRequiredNetworkType(TaskInfo.NETWORK_TYPE_UNMETERED).build());
        assertEquals(JobInfo.NETWORK_TYPE_UNMETERED, jobInfo.getNetworkType());

        jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                getInstrumentation().getTargetContext(),
                taskBuilder.setRequiresCharging(true).build());
        assertTrue(jobInfo.isRequireCharging());
    }
}
