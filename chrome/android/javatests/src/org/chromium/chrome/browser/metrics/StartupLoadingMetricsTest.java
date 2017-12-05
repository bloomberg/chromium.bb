// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

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
    private static final String FIRST_COMMIT_HISTOGRAM =
            "Startup.Android.Experimental.Cold.TimeToFirstNavigationCommit";

    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
    }

    /**
     * Tests that the startup time to first navigation commit histograms are recorded only once on
     * startup.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testNavigationCommitUmaRecorded() throws InterruptedException {
        mActivityTestRule.startMainActivityWithURL(mTestPage);
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        mActivityTestRule.loadUrl(mTestPage2);
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
    }

    /**
     * Tests that the startup time to first navigation commit histograms are recorded in case of
     * intent coming from an external app.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testNavigationCommitUmaFromExternalAppRecorded() throws InterruptedException {
        mActivityTestRule.startMainActivityFromExternalApp(mTestPage, null);
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        mActivityTestRule.loadUrl(mTestPage2);
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
    }

    /**
     * Tests that the startup time to first navigation commit histograms are not recorded in case of
     * navigation to the NTP.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testNavigationCommitUmaNTPNotRecorded() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        mActivityTestRule.loadUrl(mTestPage2);
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
    }

    /**
     * Tests that the startup time to first navigation commit histograms are not recorded in case of
     * navigation to the blank page.
     */
    @Test
    @LargeTest
    @RetryOnFailure
    public void testNavigationCommitUmaBlankPageNotRecorded() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        mActivityTestRule.loadUrl(mTestPage2);
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
    }
}