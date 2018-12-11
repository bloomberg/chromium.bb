// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import com.google.android.gms.gcm.TaskParams;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.chrome.browser.ServicificationBackgroundService;
import org.chromium.chrome.browser.init.ServiceManagerStartupUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServerRule;

/**
 * Tests interrupted download can be resumed with Service Manager only mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
public final class ServicificationDownloadTest {
    @Rule
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    private static final String TEST_DOWNLOAD_FILE = "/chrome/test/data/android/download/test.gzip";
    private static final String DOWNLOAD_GUID = "F7FB1F59-7DE1-4845-AFDB-8A688F70F583";
    private ServicificationBackgroundService mServicificationBackgroundService;
    private MockDownloadNotificationService mNotificationService;

    static class MockDownloadNotificationService extends DownloadNotificationService {
        private boolean mDownloadCompleted;

        @Override
        public int notifyDownloadSuccessful(ContentId id, String filePath, String fileName,
                long systemDownloadId, boolean isOffTheRecord, boolean isSupportedMimeType,
                boolean isOpenable, Bitmap icon, String originalUrl, boolean shouldPromoteOrigin,
                String referrer, long totalBytes) {
            mDownloadCompleted = true;
            return 0;
        }

        public void waitForDownloadCompletion() {
            CriteriaHelper.pollUiThread(
                    new Criteria("Failed waiting for the download to complete.") {
                        @Override
                        public boolean isSatisfied() {
                            return mDownloadCompleted;
                        }
                    });
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        RecordHistogram.setDisabledForTests(true);
        mServicificationBackgroundService = new ServicificationBackgroundService();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mNotificationService = new MockDownloadNotificationService(); });
    }

    @After
    public void tearDown() throws Exception {
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    @LargeTest
    @Feature({"Download"})
    @CommandLineFlags.Add({"enable-features=NetworkService,AllowStartingServiceManagerOnly"})
    public void testResumeInterruptedDownload() {
        mServicificationBackgroundService.onRunTask(
                new TaskParams(ServiceManagerStartupUtils.TASK_TAG));
        mServicificationBackgroundService.waitForServiceManagerStart();
        mServicificationBackgroundService.postTaskAndVerifyFullBrowserNotStarted();

        String tempFile = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getCacheDir()
                                  .getPath()
                + "/test.gzip";
        TestFileUtil.deleteFile(tempFile);
        DownloadItem item = new DownloadItem(false,
                new DownloadInfo.Builder()
                        .setDownloadGuid(DOWNLOAD_GUID)
                        .setIsOffTheRecord(false)
                        .build());
        final String url = mEmbeddedTestServerRule.getServer().getURL(TEST_DOWNLOAD_FILE);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadManagerService downloadManagerService =
                    DownloadManagerService.getDownloadManagerService();
            ((SystemDownloadNotifier) downloadManagerService.getDownloadNotifier())
                    .setDownloadNotificationService(mNotificationService);
            downloadManagerService.createInterruptedDownloadForTest(url, DOWNLOAD_GUID, tempFile);
            downloadManagerService.resumeDownload(
                    new ContentId("download", DOWNLOAD_GUID), item, true);
        });
        mNotificationService.waitForDownloadCompletion();
    }
}
