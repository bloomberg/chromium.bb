// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;
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
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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
    private static final String FIRST_COMMIT_HISTOGRAM =
            "Startup.Android.Experimental.Cold.TimeToFirstNavigationCommit";
    private static final String FIRST_CONTENTFUL_PAINT_HISTOGRAM =
            "Startup.Android.Experimental.Cold.TimeToFirstContentfulPaint";

    private String mTestPage;
    private String mTestPage2;
    private String mErrorPage;
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
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private interface InterruptibleRunnable { void run() throws InterruptedException; }

    private void runAndWaitForPageLoadMetricsRecorded(InterruptibleRunnable runnable)
            throws InterruptedException {
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

    private void loadUrlAndWaitForPageLoadMetricsRecorded(String url) throws InterruptedException {
        runAndWaitForPageLoadMetricsRecorded(() -> mActivityTestRule.loadUrl(url));
    }

    private void assertHistogramsRecorded(int expectedCount) throws InterruptedException {
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
    public void testStartWithURLRecorded() throws InterruptedException {
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
    public void testFromExternalAppRecorded() throws InterruptedException {
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
    public void testNTPNotRecorded() throws InterruptedException {
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
    public void testBlankPageNotRecorded() throws InterruptedException {
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
    public void testErrorPageNotRecorded() throws InterruptedException {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mActivityTestRule.startMainActivityWithURL(mErrorPage));
        assertHistogramsRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTestPage2);
        assertHistogramsRecorded(0);
    }
}
