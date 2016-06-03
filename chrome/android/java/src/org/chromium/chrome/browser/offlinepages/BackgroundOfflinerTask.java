// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.os.Bundle;

/**
 * Handles servicing of background offlining requests coming via the GcmNetworkManager.
 */
public class BackgroundOfflinerTask implements BackgroundSchedulerBridge.ProcessingDoneCallback {
    private static final String TAG = "BackgroundTask";

    /**
     * Triggers processing of background offlining requests.  This is called when
     * system conditions are appropriate for background offlining, typically from the
     * GcmTaskService onRunTask() method.  In response, we will start the
     * task processing by passing the call along to the C++ RequestCoordinator.
     * Also starts UMA collection.
     *
     * @returns true for success
     */
    public static boolean startBackgroundRequests(Context context, Bundle bundle) {
        BackgroundOfflinerTask task = new BackgroundOfflinerTask();
        task.processBackgroundRequests(bundle);

        // Gather UMA data to measure how often the user's machine is amenable to background
        // loading when we wake to do a task.
        long millisSinceBootTask = bundle.getLong(BackgroundScheduler.DATE_TAG);
        OfflinePageUtils.recordWakeupUMA(context, millisSinceBootTask);

        return true;
    }

    /**
     * Triggers processing of background offlining requests.
     */
    private void processBackgroundRequests(Bundle bundle) {
        // TODO(petewil): Nothing is holding the Wake Lock.  We need some solution to
        // keep hold of it.  Options discussed so far are having a fresh set of functions
        // to grab and release a countdown latch, or holding onto the wake lock ourselves,
        // or grabbing the wake lock and then starting chrome and running startProcessing
        // on the UI thread.

        // TODO(petewil): Decode the TriggerConditions from the bundle.

        // Pass the activation on to the bridge to the C++ RequestCoordinator.
        // Callback will hold onto the reference keeping this instance alive.
        BackgroundSchedulerBridge.startProcessing(this);
    }

    /**
     * Callback function which indicates completion of background work.
     * @param result - true if work was actually done (used for UMA).
     */
    @Override
    public void onProcessingDone(boolean result) {

    }
}
