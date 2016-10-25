// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;

import java.util.UUID;

/**
 * Tests of {@link SystemDownloadNotifier}.
 */
public class SystemDownloadNotifierTest extends InstrumentationTestCase {
    private MockSystemDownloadNotifier mDownloadNotifier;
    private MockDownloadNotificationService mService;

    static class MockSystemDownloadNotifier extends SystemDownloadNotifier {
        boolean mStarted = false;

        MockSystemDownloadNotifier(Context context) {
            super(context);
        }

        @Override
        void startService() {
            mStarted = true;
        }

        @Override
        void stopService() {
            mStarted = false;
        }

        @Override
        void onSuccessNotificationShown(
                final SystemDownloadNotifier.PendingNotificationInfo notificationInfo,
                final int notificationId) {
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mDownloadNotifier = new MockSystemDownloadNotifier(getInstrumentation().getTargetContext());
    }

    /**
     * Helper method to simulate that the DownloadNotificationService is connected.
     */
    private void onServiceConnected() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mService = new MockDownloadNotificationService();
                mService.setContext(new AdvancedMockContext(
                        getInstrumentation().getTargetContext().getApplicationContext()));
                mService.onCreate();
            }
        });
        mDownloadNotifier.setDownloadNotificationService(mService);
        mDownloadNotifier.handlePendingNotifications();
    }

    /**
     * Tests that pending notifications will be handled after service is connected.
     */
    @SmallTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testNotificationNotHandledUntilServiceConnection() {
        DownloadInfo info = new DownloadInfo.Builder()
                .setDownloadGuid(UUID.randomUUID().toString()).build();
        mDownloadNotifier.notifyDownloadProgress(info, 1L, true);
        assertTrue(mDownloadNotifier.mStarted);

        onServiceConnected();
        assertEquals(1, mService.getNotificationIds().size());
    }

    /**
     * Tests that service will be stopped once all notifications are inactive.
     */
    @SmallTest
    @Feature({"Download"})
    public void testServiceStoppedWhenAllDownloadsFinish() {
        onServiceConnected();
        DownloadInfo info = new DownloadInfo.Builder()
                .setDownloadGuid(UUID.randomUUID().toString()).build();
        mDownloadNotifier.notifyDownloadProgress(info, 1L, true);
        assertTrue(mDownloadNotifier.mStarted);
        DownloadInfo info2 = new DownloadInfo.Builder()
                .setDownloadGuid(UUID.randomUUID().toString()).build();
        mDownloadNotifier.notifyDownloadProgress(info2, 1L, true);

        mDownloadNotifier.notifyDownloadFailed(info);
        assertTrue(mDownloadNotifier.mStarted);
        mDownloadNotifier.notifyDownloadSuccessful(info2, 100L, true, false);
        assertFalse(mDownloadNotifier.mStarted);
    }
}
