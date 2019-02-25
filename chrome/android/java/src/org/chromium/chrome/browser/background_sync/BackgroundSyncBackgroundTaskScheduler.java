// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import android.os.Bundle;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * The {@link BackgroundSyncBackgroundTaskScheduler} singleton is responsible
 * for scheduling and cancelling background tasks to wake Chrome up so that
 * Background Sync events ready to be fired can be fired.
 *
 * Thread model: This class is to be run on the UI thread only.
 */
public class BackgroundSyncBackgroundTaskScheduler {
    // Keep in sync with the default min_sync_recovery_time of
    // BackgroundSyncParameters.
    private static final long MIN_SYNC_RECOVERY_TIME = DateUtils.MINUTE_IN_MILLIS * 6;

    // Bundle key for the timestamp of the soonest wakeup time expected for
    // this task.
    public static final String SOONEST_EXPECTED_WAKETIME = "SoonestWakeupTime";

    private static class LazyHolder {
        static final BackgroundSyncBackgroundTaskScheduler INSTANCE =
                new BackgroundSyncBackgroundTaskScheduler();
    }

    @CalledByNative
    public static BackgroundSyncBackgroundTaskScheduler getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Cancels a task with Id BACKGROUND_SYNC_ONE_SHOT_JOB_ID, if there's one
     * scheduled.
     */
    @VisibleForTesting
    protected void cancelOneShotTask() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID);
    }

    /**
     * Schedules a one-off bakground task to wake the browser up on network
     * connectivity. There is a delay of of six minutes before waking the
     * browser.
     */
    protected boolean scheduleOneShotTask() {
        return scheduleOneShotTask(MIN_SYNC_RECOVERY_TIME);
    }

    /**
     * Schedules a one-off background task to wake the browser up on network
     * connectivity and call into native code to fire ready Background Sync
     * events.
     * @param minDelayMs The minimum time to wait before waking the browser.
     */
    protected boolean scheduleOneShotTask(long minDelayMs) {
        // Pack SOONEST_EXPECTED_WAKETIME in extras.
        Bundle taskExtras = new Bundle();
        taskExtras.putLong(SOONEST_EXPECTED_WAKETIME, System.currentTimeMillis() + minDelayMs);

        TaskInfo taskInfo =
                TaskInfo.createOneOffTask(TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID,
                                BackgroundSyncBackgroundTask.class, minDelayMs, Integer.MAX_VALUE)
                        .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                        .setUpdateCurrent(true)
                        .setIsPersisted(true)
                        .setExtras(taskExtras)
                        .build();
        // This will overwrite any existing task with this ID.
        return BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfo);
    }

    /**
     * Based on shouldLaunch, either creates or cancels a one-off background task
     * to wake up Chrome upon network connectivity.
     * @param shouldLaunch Whether to launch the browser in the background.
     * @param minDelayMs The minimum time to wait before waking the browser.
     */
    @VisibleForTesting
    @CalledByNative
    protected void launchBrowserIfStopped(boolean shouldLaunch, long minDelayMs) {
        if (!shouldLaunch) {
            cancelOneShotTask();
            return;
        }

        scheduleOneShotTask(minDelayMs);
    }

    /**
     * Method for rescheduling a background task to wake up Chrome for processing
     * one-shot Background Sync events in the event of an OS upgrade or
     * Google Play Services upgrade.
     */
    public void reschedule() {
        scheduleOneShotTask(MIN_SYNC_RECOVERY_TIME);
    }
}
