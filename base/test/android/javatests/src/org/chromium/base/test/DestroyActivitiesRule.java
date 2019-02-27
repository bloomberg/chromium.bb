// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.os.Build;

import org.junit.rules.ExternalResource;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeoutException;

/**
 * This is to ensure all calls to onDestroy() are performed before starting the next test.
 * We could probably remove this when crbug.com/932130 is fixed.
 */
public class DestroyActivitiesRule extends ExternalResource {
    private static final String TAG = "DestroyActivities";
    @Override
    public void after() {
        if (ApplicationStatus.isInitialized()) {
            CallbackHelper allDestroyedCalledback = new CallbackHelper();
            ApplicationStatus.ActivityStateListener activityStateListener =
                    (activity, newState) -> {
                switch (newState) {
                    case ActivityState.DESTROYED:
                        if (ApplicationStatus.isEveryActivityDestroyed()) {
                            allDestroyedCalledback.notifyCalled();
                        }
                        break;
                    case ActivityState.CREATED:
                        if (!activity.isFinishing()) {
                            activity.finish();
                        }
                        break;
                }
            };
            ApplicationStatus.registerStateListenerForAllActivities(activityStateListener);
            ThreadUtils.runOnUiThread(() -> {
                for (Activity a : ApplicationStatus.getRunningActivities()) {
                    if (!a.isFinishing()) {
                        a.finish();
                    }
                }
            });
            try {
                if (!ApplicationStatus.isEveryActivityDestroyed()) {
                    allDestroyedCalledback.waitForCallback();
                }
            } catch (InterruptedException | TimeoutException e) {
                // There appears to be a framework bug on KitKat where onStop and onDestroy are not
                // called for a handful of tests.
                if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
                    Log.w(TAG, "Activity failed to be destroyed after a test");
                    for (Activity a : ApplicationStatus.getRunningActivities()) {
                        ApplicationStatus.onStateChangeForTesting(a, ActivityState.DESTROYED);
                    }
                    throw new RuntimeException(e);
                }
            } finally {
                ApplicationStatus.unregisterActivityStateListener(activityStateListener);
            }
        }
    }
}
