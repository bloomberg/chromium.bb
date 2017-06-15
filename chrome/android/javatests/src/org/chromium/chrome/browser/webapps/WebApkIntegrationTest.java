// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Integration tests for WebAPK feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class WebApkIntegrationTest {
    @Rule
    public final ChromeActivityTestRule<WebApkActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(WebApkActivity.class);

    @Rule
    public final TopActivityListener activityListener = new TopActivityListener();

    private static final long STARTUP_TIMEOUT = ScalableTimeout.scaleTimeout(10000);

    private EmbeddedTestServer mTestServer;

    public void startWebApkActivity(String webApkPackageName, final String startUrl)
            throws InterruptedException {
        Intent intent =
                new Intent(InstrumentationRegistry.getTargetContext(), WebApkActivity.class);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, startUrl);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        WebApkActivity webApkActivity =
                (WebApkActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                        intent);
        mActivityTestRule.setActivity(webApkActivity);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getActivityTab() != null;
            }
        }, STARTUP_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), startUrl);
    }

    /** Waits for the splash screen to be hidden. */
    public void waitUntilSplashscreenHides() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mActivityTestRule.getActivity().isSplashScreenVisibleForTests();
            }
        });
    }

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        WebApkUpdateManager.setUpdatesEnabledForTesting(false);
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Test launching a WebAPK. Test that loading the start page works and that the splashscreen
     * eventually hides.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndNavigateOffOrigin() throws Exception {
        startWebApkActivity("org.chromium.webapk.test",
                mTestServer.getURL("/chrome/test/data/android/test.html"));
        waitUntilSplashscreenHides();

        // We navigate outside origin and expect Custom Tab to open on top of WebApkActivity.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.top.location = 'https://www.google.com/'");

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ChromeActivity activity = (ChromeActivity) activityListener.getMostRecentActivity();
                return activity instanceof CustomTabActivity
                        && activity.getActivityTab() != null
                        // Dropping the TLD as Google can redirect to a local site.
                        && activity.getActivityTab().getUrl().startsWith("https://www.google.");
            }
        });
    }
}
