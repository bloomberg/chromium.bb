// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.annotation.TargetApi;
import android.os.Build;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

/**
 * A factory for {@link BackgroundTaskScheduler} that ensures there is only ever a single instance.
 */
public final class BackgroundTaskSchedulerFactory {
    private static BackgroundTaskScheduler sInstance;

    @TargetApi(Build.VERSION_CODES.M)
    private static BackgroundTaskSchedulerDelegate getSchedulerDelegate() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return new BackgroundTaskSchedulerJobService();
        } else {
            return new BackgroundTaskSchedulerGcmNetworkManager();
        }
    }

    /**
     * @return the current instance of the {@link BackgroundTaskScheduler}. Creates one if none
     * exist.
     */
    public static BackgroundTaskScheduler getScheduler() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new BackgroundTaskScheduler(getSchedulerDelegate());
        return sInstance;
    }

    @VisibleForTesting
    static void setSchedulerForTesting(BackgroundTaskScheduler scheduler) {
        sInstance = scheduler;
    }

    // Do not instantiate.
    private BackgroundTaskSchedulerFactory() {}
}
