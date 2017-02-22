// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.os.Build;
import android.os.PersistableBundle;

import java.util.concurrent.TimeUnit;

/**
 * The background job scheduler class used for scheduling tasks using JobScheduler.
 */
@TargetApi(Build.VERSION_CODES.N)
public class BackgroundJobScheduler extends BackgroundScheduler {
    public static final int JOB_ID = 774322033;

    public BackgroundJobScheduler(Context context) {
        super(context);
    }

    @Override
    public void cancel() {
        getJobScheduler().cancel(JOB_ID);
    }

    @Override
    protected void scheduleImpl(TriggerConditions triggerConditions, long delayStartSeconds,
            long executionDeadlineSeconds, boolean overwrite) {
        if (!overwrite) {
            JobInfo existingJob = getJobScheduler().getPendingJob(JOB_ID);
            if (existingJob != null) return;
        }

        PersistableBundle taskExtras = new PersistableBundle();
        TaskExtrasPacker.packTimeInBundle(taskExtras);
        TaskExtrasPacker.packTriggerConditionsInBundle(taskExtras, triggerConditions);

        JobInfo jobInfo =
                new JobInfo
                        .Builder(JOB_ID, new ComponentName(
                                                 getContext(), BackgroundSchedulerJobService.class))
                        .setMinimumLatency(TimeUnit.SECONDS.toMillis(delayStartSeconds))
                        .setOverrideDeadline(TimeUnit.SECONDS.toMillis(executionDeadlineSeconds))
                        .setPersisted(true)  // Across device resets.
                        .setRequiredNetworkType(triggerConditions.requireUnmeteredNetwork()
                                        ? JobInfo.NETWORK_TYPE_UNMETERED
                                        : JobInfo.NETWORK_TYPE_ANY)
                        .setRequiresCharging(triggerConditions.requirePowerConnected())
                        .setExtras(taskExtras)
                        .build();

        getJobScheduler().schedule(jobInfo);
    }

    private JobScheduler getJobScheduler() {
        return (JobScheduler) getContext().getSystemService(Context.JOB_SCHEDULER_SERVICE);
    }
}
