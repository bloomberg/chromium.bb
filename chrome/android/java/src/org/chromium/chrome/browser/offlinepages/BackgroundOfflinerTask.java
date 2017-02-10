// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.ChromeBackgroundServiceWaiter;
import org.chromium.chrome.browser.offlinepages.interfaces.BackgroundSchedulerProcessor;

/**
 * Handles servicing of background offlining requests coming via the GcmNetworkManager.
 */
public class BackgroundOfflinerTask {
    private static final String TAG = "BGOfflinerTask";
    private static final long DEFER_START_SECONDS = 5 * 60;

    public BackgroundOfflinerTask(BackgroundSchedulerProcessor bridge) {
        mBridge = bridge;
    }

    private final BackgroundSchedulerProcessor mBridge;

    /**
     * Triggers processing of background offlining requests.  This is called when
     * system conditions are appropriate for background offlining, typically from the
     * GcmTaskService onRunTask() method.  In response, we will start the
     * task processing by passing the call along to the C++ RequestCoordinator.
     * Also starts UMA collection.
     *
     * @returns true for success
     */
    public void startBackgroundRequests(
            Context context, Bundle bundle, final ChromeBackgroundServiceWaiter waiter) {
        // Complete the wait if background request processing was not started.
        // If background processing was started, completion is going to be handled by callback.
        if (!startBackgroundRequestsImpl(context, bundle, waiter)) {
            waiter.onWaitDone();
        }
    }

    /**
     * Triggers processing of background offlining requests.  This is called when
     * system conditions are appropriate for background offlining, typically from the
     * GcmTaskService onRunTask() method.  In response, we will start the
     * task processing by passing the call along to the C++ RequestCoordinator.
     * Also starts UMA collection.
     *
     * @returns true for success
     */
    private boolean startBackgroundRequestsImpl(
            Context context, Bundle bundle, final ChromeBackgroundServiceWaiter waiter) {
        // Set up backup scheduled task in case processing is killed before RequestCoordinator
        // has a chance to reschedule base on remaining work.
        TriggerConditions previousTriggerConditions =
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(bundle);
        BackgroundScheduler.backupSchedule(context, previousTriggerConditions, DEFER_START_SECONDS);

        DeviceConditions currentConditions = OfflinePageUtils.getDeviceConditions(context);
        if (!currentConditions.isPowerConnected()
                && currentConditions.getBatteryPercentage()
                        < previousTriggerConditions.getMinimumBatteryPercentage()) {
            Log.d(TAG, "Battery percentage is lower than minimum to start processing");
            return false;
        }

        if (SysUtils.isLowEndDevice() && ApplicationStatus.hasVisibleActivities()) {
            Log.d(TAG, "Application visible on low-end device so deferring background processing");
            return false;
        }

        // Gather UMA data to measure how often the user's machine is amenable to background
        // loading when we wake to do a task.
        long taskScheduledTimeMillis = TaskExtrasPacker.unpackTimeFromBundle(bundle);
        OfflinePageUtils.recordWakeupUMA(context, taskScheduledTimeMillis);

        return mBridge.startScheduledProcessing(currentConditions, createCallback(waiter));
    }

    private Callback<Boolean> createCallback(final ChromeBackgroundServiceWaiter waiter) {
        return new Callback<Boolean>() {
            /** Callback releasing the wakelock once background work concludes. */
            @Override
            public void onResult(Boolean result) {
                Log.d(TAG, "onProcessingDone");
                // Release the wake lock.
                waiter.onWaitDone();
            }
        };
    }
}
