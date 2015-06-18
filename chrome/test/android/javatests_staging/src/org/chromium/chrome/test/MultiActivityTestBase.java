// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.text.TextUtils;

import junit.framework.Assert;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.Tab;
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
    protected static final String URL_1 = createTestUrl(1);
    protected static final String URL_2 = createTestUrl(2);
    protected static final String URL_3 = createTestUrl(3);
    protected static final String URL_4 = createTestUrl(4);

    /** Defines one gigantic link spanning the whole page that creates a new window with URL_4. */
    protected static final String HREF_LINK = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <title>href link page</title>"
            + "    <meta name='viewport'"
            + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
            + "    <style>"
            + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
            + "    </style>"
            + "  </head>"
            + "  <body>"
            + "    <a href='" + URL_4 + "' target='_blank'><div></div></a>"
            + "  </body>"
            + "</html>");

    /** Clicking the body triggers a window.open() call to open URL_4. */
    protected static final String SUCCESS_URL = UrlUtils.encodeHtmlDataUri("opened!");
    protected static final String ONCLICK_LINK = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <title>window.open page</title>"
            + "    <meta name='viewport'"
            + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
            + "    <style>"
            + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
            + "    </style>"
            + "    <script>"
            + "      function openNewWindow() {"
            + "        if (window.open('" + URL_4 + "')) location.href = '" + SUCCESS_URL + "';"
            + "      }"
            + "    </script>"
            + "  </head>"
            + "  <body id='body'>"
            + "    <div onclick='openNewWindow()'></div></a>"
            + "  </body>"
            + "</html>");

    private static final float FLOAT_EPSILON = 0.001f;

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

    /**
     * Approximates when a ChromeActivity is fully ready and loaded, which is hard to gauge
     * because Android's Activity transition animations are not monitorable.
     */
    protected void waitForFullLoad(final ChromeActivity activity, final String expectedTitle)
            throws Exception {
        assertWaitForPageScaleFactorMatch(activity, 0.5f);
        final Tab tab = activity.getActivityTab();
        assert tab != null;

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!tab.isLoadingAndRenderingDone()) return false;
                if (!TextUtils.equals(expectedTitle, tab.getTitle())) return false;
                return true;
            }
        }));
    }

    /**
     * Proper use of this function requires waiting for a page scale factor that isn't 1.0f because
     * the default seems to be 1.0f.
     * TODO(dfalcantara): Combine this one and ChromeActivityTestCaseBase's (crbug.com/498973)
     */
    private void assertWaitForPageScaleFactorMatch(
            final ChromeActivity activity, final float expectedScale) throws Exception {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (activity.getCurrentContentViewCore() == null) return false;

                return Math.abs(activity.getCurrentContentViewCore().getScale() - expectedScale)
                        < FLOAT_EPSILON;
            }
        }));
    }

    private static final String createTestUrl(int index) {
        String[] colors = {"#000000", "#ff0000", "#00ff00", "#0000ff", "#ffff00"};
        return UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <title>Page " + index + "</title>"
            + "    <meta name='viewport' content='width=device-width "
            + "        initial-scale=0.5 maximum-scale=0.5'>"
            + "  </head>"
            + "  <body style='margin: 0em; background: " + colors[index] + ";'></body>"
            + "</html>");
    }
}
