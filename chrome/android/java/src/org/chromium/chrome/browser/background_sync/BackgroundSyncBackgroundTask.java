// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import android.content.Context;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerPrefs;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * Handles servicing of Background Sync background tasks coming via
 * background_task_scheduler component.
 */
public class BackgroundSyncBackgroundTask extends NativeBackgroundTask {
    @Override
    public @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID;

        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        // Record the delay from soonest expected wakeup time.
        long delayFromExpectedMs = System.currentTimeMillis()
                - taskParameters.getExtras().getLong(
                        BackgroundSyncBackgroundTaskScheduler.SOONEST_EXPECTED_WAKETIME);
        RecordHistogram.recordLongTimesHistogram(
                "BackgroundSync.Wakeup.DelayTime", delayFromExpectedMs);

        // Now that Chrome has been started, BackgroundSyncManager will
        // eventually be created, and it'll fire any ready sync events.
        // It'll also schedule a background task with the required delay.
        // In case Chrome gets closed before native code gets to run,
        // schedule a task to wake up Chrome with a delay, as a backup. This is
        // done only if there isn't already a similar task scheduled. This'll
        // be overwritten by a similar call from BackgroundSyncManager.
        if (!BackgroundTaskSchedulerPrefs.getScheduledTasks().contains(
                    BackgroundSyncBackgroundTask.class.getName())) {
            BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneShotTask();
        }
        callback.taskFinished(true);
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID;

        // Native didn't complete loading, but it was supposed to.
        // Presume we need to reschedule.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID;

        // Don't reschedule again.
        return false;
    }

    @Override
    public void reschedule(Context context) {
        BackgroundSyncBackgroundTaskScheduler.getInstance().reschedule();
    }
}
