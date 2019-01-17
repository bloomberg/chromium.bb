// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.annotation.SuppressLint;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskInfo.NetworkType;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Class for scheduing download resumption tasks.
 */
public class DownloadResumptionScheduler {
    @SuppressLint("StaticFieldLeak")
    private static DownloadResumptionScheduler sDownloadResumptionScheduler;

    public static DownloadResumptionScheduler getDownloadResumptionScheduler() {
        if (sDownloadResumptionScheduler == null) {
            sDownloadResumptionScheduler = new DownloadResumptionScheduler();
        }
        return sDownloadResumptionScheduler;
    }

    protected DownloadResumptionScheduler() {}

    /**
     * Checks the persistence layer and schedules a task to restart the app and resume any downloads
     * if there are resumable downloads available.
     */
    public void scheduleIfNecessary() {
        if (FeatureUtilities.isDownloadAutoResumptionEnabledInNative()) return;

        List<DownloadSharedPreferenceEntry> entries =
                DownloadSharedPreferenceHelper.getInstance().getEntries();

        boolean scheduleAutoResumption = false;
        boolean allowMeteredConnection = false;
        for (int i = 0; i < entries.size(); ++i) {
            DownloadSharedPreferenceEntry entry = entries.get(i);
            if (entry.isAutoResumable) {
                scheduleAutoResumption = true;
                if (entry.canDownloadWhileMetered) {
                    allowMeteredConnection = true;
                    break;
                }
            }
        }

        if (scheduleAutoResumption) {
            @NetworkType
            int networkType = allowMeteredConnection ? TaskInfo.NETWORK_TYPE_ANY
                                                     : TaskInfo.NETWORK_TYPE_UNMETERED;

            TaskInfo task = TaskInfo.createOneOffTask(TaskIds.DOWNLOAD_RESUMPTION_JOB_ID,
                                            DownloadResumptionBackgroundTask.class,
                                            TimeUnit.DAYS.toMillis(1))
                                    .setUpdateCurrent(true)
                                    .setRequiredNetworkType(networkType)
                                    .setRequiresCharging(false)
                                    .setIsPersisted(true)
                                    .build();

            BackgroundTaskSchedulerFactory.getScheduler().schedule(
                    ContextUtils.getApplicationContext(), task);
        } else {
            cancel();
        }
    }

    /**
     * Cancels any outstanding task that could restart the app and resume downloads.
     */
    public void cancel() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), TaskIds.DOWNLOAD_RESUMPTION_JOB_ID);
    }

    /**
     * Kicks off the download resumption process through {@link DownloadNotificationService},
     * which handles actually resuming the individual downloads.
     *
     * It is assumed that native is loaded at the time of this call.
     */
    public void resume() {
        DownloadNotificationService.getInstance().resumeAllPendingDownloads();
    }
}
