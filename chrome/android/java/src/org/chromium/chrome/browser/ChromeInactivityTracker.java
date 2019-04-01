// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Manages pref that can track the delay since the last stop of the tracked activity.
 */
public class ChromeInactivityTracker implements StartStopWithNativeObserver, Destroyable {
    private static final String TAG = "InactivityTracker";
    private static final String FEATURE_NAME = ChromeFeatureList.NTP_LAUNCH_AFTER_INACTIVITY;
    private static final String NTP_LAUNCH_DELAY_IN_MINS_PARAM = "delay_in_mins";
    private static final long UNKNOWN_LAST_BACKGROUNDED_TIME = -1;
    private static final int UNKNOWN_LAUNCH_DELAY_MINS = -1;
    private static final int DEFAULT_LAUNCH_DELAY_IN_MINS = 5;

    // Only true if the feature is enabled.
    private final boolean mIsEnabled;

    private final String mPrefName;
    private int mNtpLaunchDelayInMins = 1;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Runnable mInactiveCallback;
    private CancelableRunnableTask mCurrentlyPostedInactiveCallback;

    private static class CancelableRunnableTask implements Runnable {
        private boolean mIsRunnable = true;
        private final Runnable mTask;

        private CancelableRunnableTask(Runnable task) {
            mTask = task;
        }

        @Override
        public void run() {
            if (mIsRunnable) {
                mTask.run();
            }
        }

        public void cancel() {
            mIsRunnable = false;
        }
    }

    /**
     * Creates an inactivity tracker without a timeout callback. This is useful if clients only
     * want to query the inactivity state manually.
     * @param prefName the location in shared preferences that the timestamp is stored.
     * @param lifecycleDispatcher tracks the lifecycle of the Activity of interest, and calls
     *     observer methods on ChromeInactivityTracker.
     */
    public ChromeInactivityTracker(
            String prefName, ActivityLifecycleDispatcher lifecycleDispatcher) {
        this(prefName, lifecycleDispatcher, () -> {});
    }

    /**
     * Creates an inactivity tracker that stores a timestamp in prefs, and sets a timeout. If the
     * timeout expires while the activity that is tracked is still stopped, then the callback is
     * executed.  If the activity otherwise starts up, it can check whether the timeout has expired
     * using #inactivityThresholdPassed and perform the appropriate behavior.
     * @param prefName the location in shared preferences that the timestamp is stored.
     * @param lifecycleDispatcher tracks the lifecycle of the Activity of interest, and calls
     *     observer methods on ChromeInactivityTracker.
     * @param inactiveCallback called if the activity is stopped for longer than the configured
     *     inactivity timeout.
     */
    public ChromeInactivityTracker(String prefName, ActivityLifecycleDispatcher lifecycleDispatcher,
            Runnable inactiveCallback) {
        mPrefName = prefName;
        mInactiveCallback = inactiveCallback;

        mNtpLaunchDelayInMins = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                FEATURE_NAME, NTP_LAUNCH_DELAY_IN_MINS_PARAM, DEFAULT_LAUNCH_DELAY_IN_MINS);

        mIsEnabled = ChromeFeatureList.isEnabled(FEATURE_NAME);
        if (mIsEnabled) {
            mLifecycleDispatcher = lifecycleDispatcher;
            mLifecycleDispatcher.register(this);
        } else {
            mLifecycleDispatcher = null;
        }
    }

    /**
     * Updates the shared preferences to contain the given time. Used internally and for tests.
     * @param timeInMillis the time to record.
     */
    @VisibleForTesting
    public void setLastBackgroundedTimeInPrefs(long timeInMillis) {
        ContextUtils.getAppSharedPreferences().edit().putLong(mPrefName, timeInMillis).apply();
    }

    private long getLastBackgroundedTimeMs() {
        return ContextUtils.getAppSharedPreferences().getLong(
                mPrefName, UNKNOWN_LAST_BACKGROUNDED_TIME);
    }

    /**
     * @return the time interval in millis since the last backgrounded time.
     */
    public long getTimeSinceLastBackgroundedMs() {
        long lastBackgroundedTimeMs = getLastBackgroundedTimeMs();
        if (lastBackgroundedTimeMs == UNKNOWN_LAST_BACKGROUNDED_TIME) {
            return UNKNOWN_LAST_BACKGROUNDED_TIME;
        }
        return System.currentTimeMillis() - lastBackgroundedTimeMs;
    }

    /**
     * @return true if the timestamp in prefs is older than the configured timeout.
     */
    public boolean inactivityThresholdPassed() {
        if (!mIsEnabled) {
            return false;
        }

        long lastBackgroundedTimeMs = getLastBackgroundedTimeMs();
        if (lastBackgroundedTimeMs == UNKNOWN_LAST_BACKGROUNDED_TIME) return false;

        long backgroundDurationMinutes =
                getTimeSinceLastBackgroundedMs() / DateUtils.MINUTE_IN_MILLIS;

        if (backgroundDurationMinutes < mNtpLaunchDelayInMins) {
            Log.i(TAG, "Not launching NTP due to inactivity, background time: %d, launch delay: %d",
                    backgroundDurationMinutes, mNtpLaunchDelayInMins);
            return false;
        }

        Log.i(TAG, "Forcing NTP due to inactivity.");

        return true;
    }

    @Override
    public void onStartWithNative() {
        setLastBackgroundedTimeInPrefs(UNKNOWN_LAST_BACKGROUNDED_TIME);
        cancelCurrentTask();
    }

    @Override
    public void onStopWithNative() {
        Log.i(TAG, "onStop, scheduling for " + mNtpLaunchDelayInMins + " minutes");

        cancelCurrentTask();
        if (mNtpLaunchDelayInMins == UNKNOWN_LAUNCH_DELAY_MINS) {
            Log.w(TAG, "Configured with unknown launch delay, disabling.");
            return;
        }
        mCurrentlyPostedInactiveCallback = new CancelableRunnableTask(mInactiveCallback);
        setLastBackgroundedTimeInPrefs(System.currentTimeMillis());
        org.chromium.base.task.PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT,
                mCurrentlyPostedInactiveCallback,
                mNtpLaunchDelayInMins * DateUtils.MINUTE_IN_MILLIS);
    }

    @Override
    public void destroy() {
        mLifecycleDispatcher.unregister(this);
        cancelCurrentTask();
    }

    private void cancelCurrentTask() {
        if (mCurrentlyPostedInactiveCallback != null) {
            mCurrentlyPostedInactiveCallback.cancel();
            mCurrentlyPostedInactiveCallback = null;
        }
    }
}
