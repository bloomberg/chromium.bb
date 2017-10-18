// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link PageLoadMetrics}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class PageLoadMetricsTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final int PAGE_LOAD_METRICS_TIMEOUT_MS = 3000;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_TITLE = "The Google";

    private String mTestPage;
    private EmbeddedTestServer mTestServer;
    private PageLoadMetricsObserver mMetricsObserver;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);

        mMetricsObserver =
                new PageLoadMetricsObserver(mActivityTestRule.getActivity().getActivityTab());
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PageLoadMetrics.removeObserver(mMetricsObserver);
            }
        });

        mTestServer.stopAndDestroyServer();
    }

    private static class PageLoadMetricsObserver implements PageLoadMetrics.Observer {
        private final Tab mTab;
        private final CountDownLatch mFirstContentfulPaintLatch = new CountDownLatch(1);
        private final CountDownLatch mLoadEventStartLatch = new CountDownLatch(1);

        public PageLoadMetricsObserver(Tab tab) {
            mTab = tab;
        }

        @Override
        public void onFirstContentfulPaint(
                WebContents webContents, long navigationStartTick, long firstContentfulPaintMs) {
            if (webContents != mTab.getWebContents()) return;

            if (firstContentfulPaintMs > 0) mFirstContentfulPaintLatch.countDown();
        }

        @Override
        public void onLoadEventStart(
                WebContents webContents, long navigationStartTick, long loadEventStartMs) {
            if (webContents != mTab.getWebContents()) return;

            if (loadEventStartMs > 0) mLoadEventStartLatch.countDown();
        }

        public boolean waitForFirstContentfulPaintEvent() throws InterruptedException {
            return mFirstContentfulPaintLatch.await(
                    PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public boolean waitForLoadEventStartEvent() throws InterruptedException {
            return mLoadEventStartLatch.await(PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        @Override
        public void onLoadedMainResource(WebContents webContents, long dnsStartMs, long dnsEndMs,
                long connectStartMs, long connectEndMs, long requestStartMs, long sendStartMs,
                long sendEndMs) {}

        @Override
        public void onNetworkQualityEstimate(WebContents webContents, int effectiveConnectionType,
                long httpRttMs, long transportRttMs) {}
    }

    @Test
    @SmallTest
    public void testPageLoadMetricEmitted() throws InterruptedException {
        Assert.assertFalse("Tab shouldn't be loading anything before we add observer",
                mActivityTestRule.getActivity().getActivityTab().isLoading());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PageLoadMetrics.addObserver(mMetricsObserver);
            }
        });

        mActivityTestRule.loadUrl(mTestPage);

        Assert.assertTrue("First Contentful Paint should be reported",
                mMetricsObserver.waitForFirstContentfulPaintEvent());
        Assert.assertTrue("Load event start event should be reported",
                mMetricsObserver.waitForLoadEventStartEvent());
    }
}
