// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for startup timing histograms.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class StartupLoadingMetricsTest {
    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String ERROR_PAGE = "/close-socket";
    private static final String SLOW_PAGE = "/slow?2";
    private static final String FIRST_COMMIT_HISTOGRAM =
            "Startup.Android.Experimental.Cold.TimeToFirstNavigationCommit";
    private static final String FIRST_CONTENTFUL_PAINT_HISTOGRAM =
            "Startup.Android.Experimental.Cold.TimeToFirstContentfulPaint";

    private String mTestPage;
    private String mTestPage2;
    private String mErrorPage;
    private String mSlowPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws InterruptedException {
        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        mErrorPage = mTestServer.getURL(ERROR_PAGE);
        mSlowPage = mTestServer.getURL(SLOW_PAGE);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private interface CheckedRunnable { void run() throws Exception; }

    private void runAndWaitForPageLoadMetricsRecorded(CheckedRunnable runnable) throws Exception {
        PageLoadMetricsTest.PageLoadMetricsTestObserver testObserver =
                new PageLoadMetricsTest.PageLoadMetricsTestObserver();
        ThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(testObserver));
        runnable.run();
        // First Contentful Paint may be recorded asynchronously after a page load is finished, we
        // have to wait the event to occur.
        testObserver.waitForFirstContentfulPaintEvent();
        ThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(testObserver));
    }

    private void loadUrlAndWaitForPageLoadMetricsRecorded(String url) throws Exception {
        runAndWaitForPageLoadMetricsRecorded(() -> mActivityTestRule.loadUrl(url));
    }

    private void assertHistogramsRecorded(int expectedCount) {
        Assert.assertEquals(expectedCount,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        Assert.assertEquals(expectedCount,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_CONTENTFUL_PAINT_HISTOGRAM));
    }

    /**
     * Tests that the startup loading histograms are recorded only once on startup.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testStartWithURLRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mActivityTestRule.startMainActivityWithURL(mTestPage));
        assertHistogramsRecorded(1);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTestPage2);
        assertHistogramsRecorded(1);
    }

    /**
     * Tests that the startup loading histograms are recorded in case of intent coming from an
     * external app.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testFromExternalAppRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mActivityTestRule.startMainActivityFromExternalApp(mTestPage, null));
        assertHistogramsRecorded(1);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTestPage2);
        assertHistogramsRecorded(1);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the NTP.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testNTPNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mActivityTestRule.startMainActivityFromLauncher());
        assertHistogramsRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTestPage2);
        assertHistogramsRecorded(0);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the blank
     * page.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testBlankPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mActivityTestRule.startMainActivityOnBlankPage());
        assertHistogramsRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTestPage2);
        assertHistogramsRecorded(0);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the error
     * page.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testErrorPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mActivityTestRule.startMainActivityWithURL(mErrorPage));
        assertHistogramsRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTestPage2);
        assertHistogramsRecorded(0);
    }

    /**
     * Tests that the startup loading histograms are not recorded if the application is in
     * background at the time of the page loading.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testBackgroundedPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(() -> {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.addCategory(Intent.CATEGORY_LAUNCHER);

            // mSlowPage will hang for 2 seconds before sending a response. It should be enough to
            // put Chrome in background before the page is committed.
            mActivityTestRule.prepareUrlIntent(intent, mSlowPage);
            mActivityTestRule.startActivityCompletely(intent);

            // Put Chrome in background before the page is committed.
            ApplicationTestUtils.fireHomeScreenIntent(InstrumentationRegistry.getTargetContext());

            // Wait for a tab to be loaded.
            mActivityTestRule.waitForActivityNativeInitializationComplete();
            CriteriaHelper.pollUiThread(
                    () -> mActivityTestRule.getActivity().getActivityTab() != null,
                    "Tab never selected/initialized.");
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);
        });
        assertHistogramsRecorded(0);
        runAndWaitForPageLoadMetricsRecorded(() -> {
            // Put Chrome in foreground before loading a new page.
            ApplicationTestUtils.launchChrome(InstrumentationRegistry.getTargetContext());
            mActivityTestRule.loadUrl(mTestPage);
        });
        assertHistogramsRecorded(0);
    }
}
