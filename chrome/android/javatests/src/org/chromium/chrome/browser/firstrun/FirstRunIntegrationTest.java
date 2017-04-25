// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.customtabs.CustomTabsIntent;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Integration test suite for the first run experience.
 */
@CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class FirstRunIntegrationTest extends InstrumentationTestCase {
    @Override
    public void setUp() throws Exception {
        super.setUp();
        ApplicationTestUtils.setUp(getInstrumentation().getTargetContext(), true);
    }

    @Override
    public void tearDown() throws Exception {
        ApplicationTestUtils.tearDown(getInstrumentation().getTargetContext());
        super.tearDown();
    }

    @SmallTest
    public void testGenericViewIntentGoesToFirstRun() {
        final String asyncClassName = ChromeLauncherActivity.class.getName();
        runFirstRunRedirectTestForActivity(asyncClassName, new Runnable() {
            @Override
            public void run() {
                final Context context = getInstrumentation().getTargetContext();
                Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
                intent.setPackage(context.getPackageName());
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            }
        });
    }

    @SmallTest
    public void testRedirectCustomTabActivityToFirstRun() {
        final String asyncClassName = ChromeLauncherActivity.class.getName();
        runFirstRunRedirectTestForActivity(asyncClassName, new Runnable() {
            @Override
            public void run() {
                Context context = getInstrumentation().getTargetContext();
                CustomTabsIntent customTabIntent = new CustomTabsIntent.Builder().build();
                customTabIntent.intent.setPackage(context.getPackageName());
                customTabIntent.intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                customTabIntent.launchUrl(context, Uri.parse("http://test.com"));
            }
        });
    }

    @SmallTest
    public void testRedirectChromeTabbedActivityToFirstRun() {
        final String asyncClassName = ChromeTabbedActivity.class.getName();
        runFirstRunRedirectTestForActivity(asyncClassName, new Runnable() {
            @Override
            public void run() {
                final Context context = getInstrumentation().getTargetContext();
                Intent intent = new Intent();
                intent.setClassName(context, asyncClassName);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            }
        });
    }

    @SmallTest
    public void testRedirectSearchActivityToFirstRun() {
        final String asyncClassName = SearchActivity.class.getName();
        runFirstRunRedirectTestForActivity(asyncClassName, new Runnable() {
            @Override
            public void run() {
                final Context context = getInstrumentation().getTargetContext();
                Intent intent = new Intent();
                intent.setClassName(context, asyncClassName);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            }
        });
    }

    /**
     * Tests that an AsyncInitializationActivity subclass that attempts to be run without first
     * having gone through First Run kicks the user out into the FRE.
     * @param asyncClassName Name of the class to expect.
     * @param runnable       Runnable that launches the Activity.
     */
    private void runFirstRunRedirectTestForActivity(String asyncClassName, Runnable runnable) {
        final ActivityMonitor activityMonitor = new ActivityMonitor(asyncClassName, null, false);
        final ActivityMonitor freMonitor =
                new ActivityMonitor(FirstRunActivity.class.getName(), null, false);

        Instrumentation instrumentation = getInstrumentation();
        instrumentation.addMonitor(activityMonitor);
        instrumentation.addMonitor(freMonitor);
        runnable.run();

        // The original activity should be started because it was directly specified.
        final Activity original = instrumentation.waitForMonitorWithTimeout(
                activityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        assertNotNull(original);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return original.isFinishing();
            }
        });

        // Because the AsyncInitializationActivity notices that the FRE hasn't been run yet, it
        // redirects to it.  Ideally, we would grab the Activity here, but it seems that the
        // First Run Activity doesn't live long enough to be grabbed.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return freMonitor.getHits() == 1;
            }
        });
    }

    @SmallTest
    public void testHelpPageSkipsFirstRun() {
        final ActivityMonitor customTabActivityMonitor =
                new ActivityMonitor(CustomTabActivity.class.getName(), null, false);
        final ActivityMonitor freMonitor =
                new ActivityMonitor(FirstRunActivity.class.getName(), null, false);

        Instrumentation instrumentation = getInstrumentation();
        instrumentation.addMonitor(customTabActivityMonitor);
        instrumentation.addMonitor(freMonitor);

        // Fire an Intent to load a generic URL.
        final Context context = instrumentation.getTargetContext();
        CustomTabActivity.showInfoPage(context, "http://google.com");

        // The original activity should be started because it's a "help page".
        final Activity original = instrumentation.waitForMonitorWithTimeout(
                customTabActivityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        assertNotNull(original);
        assertFalse(original.isFinishing());

        // First run should be skipped for this Activity.
        assertEquals(0, freMonitor.getHits());
    }

}
