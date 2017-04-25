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
import android.os.Bundle;
import android.support.customtabs.CustomTabsIntent;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.widget.Button;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.MultiActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Integration test suite for the first run experience.
 */
@CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class FirstRunIntegrationTest extends MultiActivityTestBase {
    private FirstRunActivityTestObserver mTestObserver = new FirstRunActivityTestObserver();
    private Activity mActivity;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        FirstRunActivity.setObserverForTest(mTestObserver);
    }

    @Override
    public void tearDown() throws Exception {
        if (mActivity != null) mActivity.finish();
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

    @SmallTest
    public void testAbortFirstRun() throws Exception {
        final ActivityMonitor freMonitor =
                new ActivityMonitor(FirstRunActivity.class.getName(), null, false);
        Instrumentation instrumentation = getInstrumentation();
        instrumentation.addMonitor(freMonitor);

        final Context context = instrumentation.getTargetContext();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
        intent.setPackage(context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        // Because the AsyncInitializationActivity notices that the FRE hasn't been run yet, it
        // redirects to it.  Once the user closes the FRE, the user should be kicked back into the
        // startup flow where they were interrupted.
        mActivity = instrumentation.waitForMonitorWithTimeout(
                freMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        instrumentation.removeMonitor(freMonitor);
        ActivityMonitor activityMonitor =
                new ActivityMonitor(ChromeLauncherActivity.class.getName(), null, false);
        instrumentation.addMonitor(activityMonitor);

        assertEquals(0, mTestObserver.abortFirstRunExperienceCallback.getCallCount());
        mActivity.onBackPressed();
        mTestObserver.abortFirstRunExperienceCallback.waitForCallback(
                "FirstRunActivity didn't abort", 0);

        mActivity = instrumentation.waitForMonitorWithTimeout(
                activityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivity.isFinishing();
            }
        });
    }

    @MediumTest
    public void testClickThroughFirstRun() throws Exception {
        final ActivityMonitor freMonitor =
                new ActivityMonitor(FirstRunActivity.class.getName(), null, false);
        Instrumentation instrumentation = getInstrumentation();
        instrumentation.addMonitor(freMonitor);

        final Context context = instrumentation.getTargetContext();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
        intent.setPackage(context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        // Because the AsyncInitializationActivity notices that the FRE hasn't been run yet, it
        // redirects to it.  Once the user closes the FRE, the user should be kicked back into the
        // startup flow where they were interrupted.
        mActivity = instrumentation.waitForMonitorWithTimeout(
                freMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        instrumentation.removeMonitor(freMonitor);
        ActivityMonitor activityMonitor =
                new ActivityMonitor(ChromeTabbedActivity.class.getName(), null, false);
        instrumentation.addMonitor(activityMonitor);

        mTestObserver.flowIsKnownCallback.waitForCallback("Failed to finalize the flow", 0);
        Bundle freProperties = mTestObserver.freProperties;
        assertEquals(0, mTestObserver.updateCachedEngineCallback.getCallCount());

        // Accept the ToS.
        if (freProperties.getBoolean(FirstRunActivity.SHOW_WELCOME_PAGE)) {
            clickButton(mActivity, R.id.terms_accept, "Failed to accept ToS");
            mTestObserver.acceptTermsOfServiceCallback.waitForCallback(
                    "Failed to accept the ToS", 0);
            mTestObserver.jumpToPageCallback.waitForCallback(
                    "Failed to try moving to the next screen", 0);
        }

        // Acknowledge that Data Saver will be enabled.
        if (freProperties.getBoolean(FirstRunActivity.SHOW_DATA_REDUCTION_PAGE)) {
            int jumpCallCount = mTestObserver.jumpToPageCallback.getCallCount();
            clickButton(mActivity, R.id.next_button, "Failed to skip data saver");
            mTestObserver.jumpToPageCallback.waitForCallback(
                    "Failed to try moving to next screen", jumpCallCount);
        }

        // Don't sign in the user.
        if (freProperties.getBoolean(FirstRunActivity.SHOW_SIGNIN_PAGE)) {
            int jumpCallCount = mTestObserver.jumpToPageCallback.getCallCount();
            clickButton(mActivity, R.id.negative_button, "Failed to skip signing-in");
            mTestObserver.jumpToPageCallback.waitForCallback(
                    "Failed to try moving to next screen", jumpCallCount);
        }

        // FRE should be completed now, which will kick the user back into the interrupted flow.
        // In this case, the user gets sent to the ChromeTabbedActivity after a View Intent is
        // processed by ChromeLauncherActivity.
        mTestObserver.updateCachedEngineCallback.waitForCallback(
                "Failed to alert search widgets that an update is necessary", 0);
        mActivity = instrumentation.waitForMonitorWithTimeout(
                activityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        assertNotNull(mActivity);
    }

    private void clickButton(final Activity activity, final int id, final String message) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity.findViewById(id) != null;
            }
        });

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Button button = (Button) activity.findViewById(id);
                assertNotNull(message, button);
                button.performClick();
            }
        });
    }
}
