// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.offlinepages.interfaces.BackgroundSchedulerProcessor;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * Handles servicing background offlining requests coming via background_task_scheduler component.
 */
public class OfflineBackgroundTask implements BackgroundTask {
    BackgroundSchedulerProcessor mBackgroundProcessor;

    public OfflineBackgroundTask() {
        mBackgroundProcessor = new BackgroundSchedulerProcessorImpl();
    }

    @Override
    public boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID;
        return BackgroundOfflinerTask.startBackgroundRequestsImpl(
                mBackgroundProcessor, context, taskParameters.getExtras(), wrapCallback(callback));
    }

    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        return mBackgroundProcessor.stopScheduledProcessing();
    }

    /** Wraps the callback for code reuse */
    private Callback<Boolean> wrapCallback(final TaskFinishedCallback callback) {
        return new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                callback.taskFinished(result);
            }
        };
    }
}
