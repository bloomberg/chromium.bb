// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

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
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.download.DownloadController;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for offline page request handling. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class MHTMLPageTest implements CustomMainActivityStart {
    @Rule
    public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final int TIMEOUT_MS = 5000;
    private static final String[] TEST_FILES = new String[] {"hello.mhtml"};

    private EmbeddedTestServer mTestServer;

    static class TestDownloadNotificationService
            implements DownloadController.DownloadNotificationService {
        private Semaphore mSemaphore;

        TestDownloadNotificationService(Semaphore semaphore) {
            mSemaphore = semaphore;
        }

        @Override
        public void onDownloadCompleted(final DownloadInfo downloadInfo) {}

        @Override
        public void onDownloadUpdated(final DownloadInfo downloadInfo) {
            mSemaphore.release();
        }

        @Override
        public void onDownloadCancelled(final DownloadInfo downloadInfo) {}

        @Override
        public void onDownloadInterrupted(
                final DownloadInfo downloadInfo, boolean isAutoResumable) {}
    }

    @Before
    public void setUp() throws Exception {
        deleteTestFiles();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        deleteTestFiles();
    }

    @Override
    public void customMainActivityStart() throws InterruptedException {
        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testDownloadMHTMLPageFromServer() throws Exception {
        final String url = mTestServer.getURL("/chrome/test/data/android/hello.mhtml");
        final Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        final Semaphore semaphore = new Semaphore(0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                DownloadController.setDownloadNotificationService(
                        new TestDownloadNotificationService(semaphore));
                tab.loadUrl(new LoadUrlParams(
                        url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR));
            }
        });

        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLoadMHTMLPageFromLocalFile() throws Exception {
        String url = UrlUtils.getIsolatedTestFileUrl("chrome/test/data/android/hello.mhtml");
        mDownloadTestRule.loadUrl(url);
    }

    /**
     * Makes sure there are no files with names identical to the ones this test uses in the
     * downloads directory
     */
    private void deleteTestFiles() {
        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
    }
}
