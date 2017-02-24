// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link PageLoadMetrics}
 */
@RetryOnFailure
public class PageLoadMetricsTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    public PageLoadMetricsTest() {
        super(ChromeActivity.class);
    }

    private static final int PAGE_LOAD_METRICS_TIMEOUT_MS = 3000;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_TITLE = "The Google";

    private String mTestPage;
    private EmbeddedTestServer mTestServer;
    private PageLoadMetricsObserver mMetricsObserver;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);

        mMetricsObserver = new PageLoadMetricsObserver(getActivity().getActivityTab());
    }

    @Override
    protected void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PageLoadMetrics.removeObserver(mMetricsObserver);
            }
        });

        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
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
    }

    @SmallTest
    public void testPageLoadMetricEmitted() throws InterruptedException {
        assertFalse("Tab shouldn't be loading anything before we add observer",
                getActivity().getActivityTab().isLoading());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PageLoadMetrics.addObserver(mMetricsObserver);
            }
        });

        loadUrl(mTestPage);

        assertTrue("First Contentful Paint should be reported",
                mMetricsObserver.waitForFirstContentfulPaintEvent());
        assertTrue("Load event start event should be reported",
                mMetricsObserver.waitForLoadEventStartEvent());
    }
}
