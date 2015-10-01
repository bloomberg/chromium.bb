// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.PowerManager;

import junit.framework.Assert;
import junit.framework.AssertionFailedError;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.omaha.OmahaClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Methods used for testing Chrome at the Application-level.
 */
public class ApplicationTestUtils {
    private static final String TAG = "ApplicationTestUtils";
    private static final float FLOAT_EPSILON = 0.001f;

    private static PowerManager.WakeLock sWakeLock = null;

    // TODO(jbudorick): fix deprecation warning crbug.com/537347
    @SuppressWarnings("deprecation")
    public static void setUp(Context context, boolean clearAppData, boolean checkHttpServer)
            throws Exception {
        if (clearAppData) {
            // Clear data and remove any tasks listed in Android's Overview menu between test runs.
            Assert.assertTrue("Unable to clear the app data", clearAppData(context));
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                finishAllChromeTasks(context);
            }
        }

        // Make sure the screen is on during test runs.
        PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        sWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK
                | PowerManager.ACQUIRE_CAUSES_WAKEUP | PowerManager.ON_AFTER_RELEASE, TAG);
        sWakeLock.acquire();

        // Disable Omaha related activities.
        OmahaClient.setEnableCommunication(false);
        OmahaClient.setEnableUpdateDetection(false);

        if (checkHttpServer) TestHttpServerClient.checkServerIsUp();
    }

    public static void tearDown(Context context) throws Exception {
        Assert.assertNotNull("Uninitialized wake lock", sWakeLock);
        sWakeLock.release();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            try {
                finishAllChromeTasks(context);
            } catch (AssertionFailedError exception) {
            }
        }
    }

    /**
     * Clear all files and folders in the Chrome application directory except 'lib'.
     * The 'cache' directory is recreated as an empty directory.
     * @param context Target instrumentation context.
     * @return Whether clearing the application data was successful.
     */
    public static boolean clearAppData(Context context) throws InterruptedException {
        return ApplicationData.clearAppData(context);
    }

    /** Send the user to the Android home screen. */
    public static void fireHomeScreenIntent(Context context) throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        waitUntilChromeInBackground();
    }

    /** Simulate starting Chrome from the launcher with a Main Intent. */
    public static void launchChrome(Context context) throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(context.getPackageName());
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        waitUntilChromeInForeground();
    }

    /** Waits until Chrome is in the background. */
    public static void waitUntilChromeInBackground() throws Exception {
        Assert.assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int state = ApplicationStatus.getStateForApplication();
                return state == ApplicationState.HAS_STOPPED_ACTIVITIES
                        || state == ApplicationState.HAS_DESTROYED_ACTIVITIES;
            }
        }));
    }

    /** Waits until Chrome is in the foreground. */
    public static void waitUntilChromeInForeground() throws Exception {
        Assert.assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int state = ApplicationStatus.getStateForApplication();
                return state == ApplicationState.HAS_RUNNING_ACTIVITIES;
            }
        }));
    }

    /** Finishes all tasks Chrome has listed in Android's Overview. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static void finishAllChromeTasks(final Context context) throws Exception {
        // Close all of the tasks one by one.
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
            task.finishAndRemoveTask();
        }

        Assert.assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getNumChromeTasks(context) == 0;
            }
        }));
    }

    /** Counts how many tasks Chrome has listed in Android's Overview. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static int getNumChromeTasks(Context context) {
        int count = 0;
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        return activityManager.getAppTasks().size();
    }

    /**
     * See {@link #assertWaitForPageScaleFactorMatch(ChromeActivity,float,long)}.
     */
    public static void assertWaitForPageScaleFactorMatch(
            final ChromeActivity activity, final float expectedScale) throws InterruptedException {
        assertWaitForPageScaleFactorMatch(activity, expectedScale, false);
    }

    /**
     * Waits till the ContentViewCore receives the expected page scale factor
     * from the compositor and asserts that this happens.
     *
     * Proper use of this function requires waiting for a page scale factor that isn't 1.0f because
     * the default seems to be 1.0f.
     */
    public static void assertWaitForPageScaleFactorMatch(final ChromeActivity activity,
            final float expectedScale, boolean waitLongerForLoad) throws InterruptedException {
        long waitTimeInMs = waitLongerForLoad ? 10000 : CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
        boolean scaleFactorMatch = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (activity.getCurrentContentViewCore() == null) return false;

                return Math.abs(activity.getCurrentContentViewCore().getScale() - expectedScale)
                        < FLOAT_EPSILON;
            }
        }, waitTimeInMs, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertTrue("Expecting scale factor of: " + expectedScale + ", got: "
                    + activity.getCurrentContentViewCore().getScale(), scaleFactorMatch);
    }
}