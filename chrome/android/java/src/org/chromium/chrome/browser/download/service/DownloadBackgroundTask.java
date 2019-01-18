// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask.StartBeforeNativeResult;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.DownloadTaskType;
import org.chromium.components.download.internal.BatteryStatusListenerAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Entry point for the download service to perform desired action when the task is fired by the
 * scheduler. The scheduled task is executed for the regular profile and also for incognito profile
 * if an incognito profile exists.
 * TODO(shaktisahu): Since we probably don't need to run tasks for incognito profile, cleanup this
 * class and remove any reference to profiles.
 */
@JNINamespace("download::android")
public class DownloadBackgroundTask extends NativeBackgroundTask {
    // Helper class to track the number of pending {@link TaskFinishedCallback}s.
    private static class PendingTaskCounter {
        // Number of tasks in progress.
        public int numPendingCallbacks;

        // Whether at least one of the tasks needs reschedule.
        public boolean needsReschedule;
    }

    // Keeps track of in progress tasks which haven't invoked their {@link TaskFinishedCallback}s.
    private Map<Integer, PendingTaskCounter> mPendingTaskCounters = new HashMap<>();

    @DownloadTaskType
    private int mCurrentTaskType;

    @Override
    protected @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        boolean requiresCharging = taskParameters.getExtras().getBoolean(
                DownloadTaskScheduler.EXTRA_BATTERY_REQUIRES_CHARGING);
        int optimalBatteryPercentage = taskParameters.getExtras().getInt(
                DownloadTaskScheduler.EXTRA_OPTIMAL_BATTERY_PERCENTAGE);
        mCurrentTaskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);
        // Reschedule if minimum battery level is not satisfied.
        if (!requiresCharging
                && BatteryStatusListenerAndroid.getBatteryPercentage() < optimalBatteryPercentage) {
            return StartBeforeNativeResult.RESCHEDULE;
        }

        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, final TaskFinishedCallback callback) {
        // In case of future upgrades, we would need to build an intent for the old version and
        // validate that this code still works. This would require decoupling this immediate class
        // from native as well.

        Callback<Boolean> wrappedCallback = new Callback<Boolean>() {
            @Override
            public void onResult(Boolean needsReschedule) {
                if (mPendingTaskCounters.get(mCurrentTaskType) == null) return;

                boolean noPendingCallbacks =
                        decrementPendingCallbackCount(mCurrentTaskType, needsReschedule);
                if (noPendingCallbacks) {
                    callback.taskFinished(
                            mPendingTaskCounters.get(mCurrentTaskType).needsReschedule);
                    mPendingTaskCounters.remove(mCurrentTaskType);
                }
            }
        };

        Profile profile = supportsServiceManagerOnly()
                ? null
                : Profile.getLastUsedProfile().getOriginalProfile();
        incrementPendingCallbackCount(mCurrentTaskType);
        nativeStartBackgroundTask(profile, mCurrentTaskType, wrappedCallback);

        if (profile != null && profile.hasOffTheRecordProfile()) {
            incrementPendingCallbackCount(mCurrentTaskType);
            nativeStartBackgroundTask(
                    profile.getOffTheRecordProfile(), mCurrentTaskType, wrappedCallback);
        }
    }

    @Override
    protected boolean supportsServiceManagerOnly() {
        return mCurrentTaskType == DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_TASK
                && DownloadUtils.shouldStartServiceManagerOnly();
    }

    private void incrementPendingCallbackCount(@DownloadTaskType int taskType) {
        PendingTaskCounter taskCounter = mPendingTaskCounters.containsKey(taskType)
                ? mPendingTaskCounters.get(taskType)
                : new PendingTaskCounter();
        taskCounter.numPendingCallbacks++;
        mPendingTaskCounters.put(taskType, taskCounter);
    }

    /** @return Whether or not there are no more pending callbacks and we can notify the system. */
    private boolean decrementPendingCallbackCount(
            @DownloadTaskType int taskType, boolean needsRescuedule) {
        PendingTaskCounter taskCounter = mPendingTaskCounters.get(taskType);
        assert taskCounter != null && taskCounter.numPendingCallbacks > 0;

        taskCounter.numPendingCallbacks = Math.max(0, taskCounter.numPendingCallbacks - 1);
        taskCounter.needsReschedule |= needsRescuedule;
        return taskCounter.numPendingCallbacks == 0;
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        @DownloadTaskType
        int taskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);
        mPendingTaskCounters.remove(taskType);

        Profile profile = supportsServiceManagerOnly()
                ? null
                : Profile.getLastUsedProfile().getOriginalProfile();
        boolean needsReschedule = nativeStopBackgroundTask(profile, taskType);

        if (profile != null && profile.hasOffTheRecordProfile()) {
            needsReschedule |= nativeStopBackgroundTask(profile.getOffTheRecordProfile(), taskType);
        }

        return needsReschedule;
    }

    @Override
    public void reschedule(Context context) {
        DownloadTaskScheduler.rescheduleAllTasks();
    }

    private native void nativeStartBackgroundTask(
            Profile profile, int taskType, Callback<Boolean> callback);
    private native boolean nativeStopBackgroundTask(Profile profile, int taskType);
}
