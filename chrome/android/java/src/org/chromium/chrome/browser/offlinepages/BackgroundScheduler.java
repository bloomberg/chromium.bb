// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.os.Bundle;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.OneoffTask;
import com.google.android.gms.gcm.Task;

import org.chromium.chrome.browser.ChromeBackgroundService;

/**
 * The background scheduler class is for setting GCM Network Manager tasks.
 */
public class BackgroundScheduler {
    private static final long ONE_WEEK_IN_SECONDS = 60 * 60 * 24 * 7;

    /**
     * For the given Triggering conditions, start a new GCM Network Manager request.
     */
    public static void schedule(Context context, TriggerConditions triggerConditions) {
        // TODO(dougarnett): Use trigger conditions in task builder config.
        schedule(context, 0 /* delayStartSecs */);
    }

    /**
     * For the given Triggering conditions, start a new GCM Network Manager request allowed
     * to run after {@code delayStartSecs} seconds.
     */
    public static void schedule(Context context, long delayStartSecs) {
        // Get the GCM Network Scheduler.
        GcmNetworkManager gcmNetworkManager = GcmNetworkManager.getInstance(context);

        // TODO(petewil): Add the triggering conditions into the argument bundle.
        // Triggering conditions will include network state and charging requirements, maybe
        // also battery percentage.
        Bundle taskExtras = new Bundle();
        TaskExtrasPacker.packTimeInBundle(taskExtras);

        Task task = new OneoffTask.Builder()
                .setService(ChromeBackgroundService.class)
                .setExecutionWindow(delayStartSecs, ONE_WEEK_IN_SECONDS)
                .setTag(OfflinePageUtils.TASK_TAG)
                .setUpdateCurrent(true)
                .setRequiredNetwork(Task.NETWORK_STATE_CONNECTED)
                .setRequiresCharging(false)
                .setExtras(taskExtras)
                .build();

        gcmNetworkManager.schedule(task);
    }

    /**
     * Cancel any outstanding GCM Network Manager requests.
     */
    public static void unschedule(Context context) {
        // Get the GCM Network Scheduler.
        GcmNetworkManager gcmNetworkManager = GcmNetworkManager.getInstance(context);
        gcmNetworkManager.cancelTask(OfflinePageUtils.TASK_TAG, ChromeBackgroundService.class);
    }
}
