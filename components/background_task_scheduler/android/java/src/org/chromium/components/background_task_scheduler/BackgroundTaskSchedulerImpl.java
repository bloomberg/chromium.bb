// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.Set;

/**
 * This {@link BackgroundTaskScheduler} is the only one used in production code, and it is used to
 * schedule jobs that run in the background.
 *
 * To get an instance of this class, use {@link BackgroundTaskSchedulerFactory#getScheduler()}.
 */
class BackgroundTaskSchedulerImpl implements BackgroundTaskScheduler {
    private static final String TAG = "BkgrdTaskScheduler";

    private final BackgroundTaskSchedulerDelegate mSchedulerDelegate;

    /** Constructor only for {@link BackgroundTaskSchedulerFactory} and internal component tests. */
    BackgroundTaskSchedulerImpl(BackgroundTaskSchedulerDelegate schedulerDelegate) {
        mSchedulerDelegate = schedulerDelegate;
    }

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        ThreadUtils.assertOnUiThread();
        boolean success = mSchedulerDelegate.schedule(context, taskInfo);
        BackgroundTaskSchedulerUma.getInstance().reportTaskScheduled(taskInfo.getTaskId(), success);
        if (success) {
            BackgroundTaskSchedulerPrefs.addScheduledTask(taskInfo);
        }
        return success;
    }

    @Override
    public void cancel(Context context, int taskId) {
        ThreadUtils.assertOnUiThread();
        BackgroundTaskSchedulerUma.getInstance().reportTaskCanceled(taskId);
        BackgroundTaskSchedulerPrefs.removeScheduledTask(taskId);
        mSchedulerDelegate.cancel(context, taskId);
    }

    @Override
    public void checkForOSUpgrade(Context context) {
        int oldSdkInt = BackgroundTaskSchedulerPrefs.getLastSdkVersion();
        int newSdkInt = Build.VERSION.SDK_INT;

        if (oldSdkInt != newSdkInt) {
            // Save the current SDK version to preferences.
            BackgroundTaskSchedulerPrefs.setLastSdkVersion(newSdkInt);
        }

        // No OS upgrade detected or OS upgrade does not change delegate.
        if (oldSdkInt == newSdkInt || !osUpgradeChangesDelegateType(oldSdkInt, newSdkInt)) {
            BackgroundTaskSchedulerUma.getInstance().flushStats();
            return;
        }

        BackgroundTaskSchedulerUma.getInstance().removeCachedStats();

        // Explicitly create and invoke old delegate type to cancel all scheduled tasks.
        // All preference entries are kept until reschedule call, which removes then then.
        BackgroundTaskSchedulerDelegate oldDelegate =
                BackgroundTaskSchedulerFactory.getSchedulerDelegateForSdk(oldSdkInt);
        Set<Integer> scheduledTaskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
        for (int taskId : scheduledTaskIds) {
            oldDelegate.cancel(context, taskId);
        }

        reschedule(context);
    }

    @Override
    public void reschedule(Context context) {
        Set<String> scheduledTasksClassNames = BackgroundTaskSchedulerPrefs.getScheduledTasks();
        BackgroundTaskSchedulerPrefs.removeAllTasks();
        for (String className : scheduledTasksClassNames) {
            BackgroundTask task =
                    BackgroundTaskReflection.getBackgroundTaskFromClassName(className);
            if (task == null) {
                Log.w(TAG, "Cannot reschedule task for: " + className);
                continue;
            }

            task.reschedule(context);
        }
    }

    private boolean osUpgradeChangesDelegateType(int oldSdkInt, int newSdkInt) {
        return oldSdkInt < Build.VERSION_CODES.M && newSdkInt >= Build.VERSION_CODES.M;
    }
}
