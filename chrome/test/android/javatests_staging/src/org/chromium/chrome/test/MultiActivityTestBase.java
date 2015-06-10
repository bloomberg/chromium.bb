// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import junit.framework.Assert;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.omaha.OmahaClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Base for testing and interacting with multiple Activities (e.g. Document or Webapp Activities).
 */
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE
        })
public abstract class MultiActivityTestBase extends RestrictedInstrumentationTestCase {
    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Disable Omaha related activities.
        OmahaClient.setEnableCommunication(false);
        OmahaClient.setEnableUpdateDetection(false);

        // Kill any tasks, if we have the API for it.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Context context = getInstrumentation().getTargetContext();
            MultiActivityTestBase.finishAllChromeTasks(context);
        }
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Context context = getInstrumentation().getTargetContext();
            MultiActivityTestBase.finishAllChromeTasks(context);
        }
    }

    /** Counts how many tasks Chrome has listed in Android's Overview. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static int getNumChromeTasks(Context context) {
        int count = 0;
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        return activityManager.getAppTasks().size();
    }

    /** Finishes all tasks Chrome has listed in Android's Overview. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static void finishAllChromeTasks(final Context context) throws Exception {
        // Go to the Home screen so that Android has no good reason to keep Chrome Activities alive.
        Intent homeIntent = new Intent(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        homeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(homeIntent);

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

    /** Send the user to the home screen. */
    public static void launchHomescreenIntent(Context context) throws Exception {
        Intent homeIntent = new Intent(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        homeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(homeIntent);
        waitUntilChromeInBackground();
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

}
