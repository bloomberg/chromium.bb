// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static junit.framework.Assert.assertEquals;

import static org.chromium.chrome.browser.download.DownloadForegroundService.clearPinnedNotificationId;
import static org.chromium.chrome.browser.download.DownloadForegroundService.getPinnedNotificationId;
import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Test for DownloadForegroundService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DownloadForegroundServiceTest {
    private static final int FAKE_DOWNLOAD_ID1 = 1;
    private static final int FAKE_DOWNLOAD_ID2 = 2;

    private Notification mNotification;
    private MockDownloadForegroundService mForegroundService;

    /**
     * Implementation of DownloadForegroundService for testing.
     * Mimics behavior of DownloadForegroundService except for calls to the actual service.
     */
    public static class MockDownloadForegroundService extends DownloadForegroundService {
        boolean mIsSdkAtLeast24 = false;

        @Override
        void startForegroundInternal(int notificationId, Notification notification) {}

        @Override
        void stopForegroundInternal(int flags) {}

        @Override
        void stopForegroundInternal(boolean removeNotification) {}

        @Override
        boolean isSdkAtLeast24() {
            return mIsSdkAtLeast24;
        }
    }

    @Before
    public void setUp() {
        mForegroundService = new MockDownloadForegroundService();
        clearPinnedNotificationId();
        mNotification =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(
                                true /* preferCompat */, ChannelDefinitions.CHANNEL_ID_DOWNLOADS)
                        .setSmallIcon(org.chromium.chrome.R.drawable.ic_file_download_white_24dp)
                        .setContentTitle("fakeContentTitle")
                        .setContentText("fakeContentText")
                        .build();
    }

    @After
    public void tearDown() {
        clearPinnedNotificationId();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testTrackPinnedNotification_sdkLessThan24() {
        mForegroundService.mIsSdkAtLeast24 = false;

        // When the service is started, this notification should be pinned.
        mForegroundService.startOrUpdateForegroundService(FAKE_DOWNLOAD_ID1, mNotification);
        assertEquals(FAKE_DOWNLOAD_ID1, getPinnedNotificationId());

        // When the service gets stopped with request to detach but not kill notification (pause),
        // the notification should remain pinned.
        mForegroundService.stopDownloadForegroundService(true, false);
        assertEquals(FAKE_DOWNLOAD_ID1, getPinnedNotificationId());

        // When the service gets started again with a different notification, this should be pinned.
        mForegroundService.startOrUpdateForegroundService(FAKE_DOWNLOAD_ID2, mNotification);
        assertEquals(FAKE_DOWNLOAD_ID2, getPinnedNotificationId());

        // When the service gets stopped with request to detach and kill (complete), the
        // notification should not remain pinned.
        mForegroundService.stopDownloadForegroundService(true, true);
        assertEquals(INVALID_NOTIFICATION_ID, getPinnedNotificationId());

        // When the service is started, this notification should be pinned.
        mForegroundService.startOrUpdateForegroundService(FAKE_DOWNLOAD_ID1, mNotification);
        assertEquals(FAKE_DOWNLOAD_ID1, getPinnedNotificationId());

        // When the service gets stopped with request to not detach but to kill (cancel), the
        // notification should not remain pinned.
        mForegroundService.stopDownloadForegroundService(false, true);
        assertEquals(INVALID_NOTIFICATION_ID, getPinnedNotificationId());
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testTrackPinnedNotification_sdkAtLeast24() {
        mForegroundService.mIsSdkAtLeast24 = true;

        // When the service is started, this notification should be pinned.
        mForegroundService.startOrUpdateForegroundService(FAKE_DOWNLOAD_ID1, mNotification);
        assertEquals(FAKE_DOWNLOAD_ID1, getPinnedNotificationId());

        // When the service gets stopped with request to detach but not kill notification (pause),
        // the notification should not remain pinned.
        mForegroundService.stopDownloadForegroundService(true, false);
        assertEquals(INVALID_NOTIFICATION_ID, getPinnedNotificationId());

        // When the service gets started again with a different notification, this should be pinned.
        mForegroundService.startOrUpdateForegroundService(FAKE_DOWNLOAD_ID2, mNotification);
        assertEquals(FAKE_DOWNLOAD_ID2, getPinnedNotificationId());

        // When the service gets stopped with request to detach and kill (complete), the
        // notification should not remain pinned.
        mForegroundService.stopDownloadForegroundService(true, true);
        assertEquals(INVALID_NOTIFICATION_ID, getPinnedNotificationId());

        // When the service is started, this notification should be pinned.
        mForegroundService.startOrUpdateForegroundService(FAKE_DOWNLOAD_ID1, mNotification);
        assertEquals(FAKE_DOWNLOAD_ID1, getPinnedNotificationId());

        // When the service gets stopped with request to not detach but to kill (cancel), the
        // notification should not remain pinned.
        mForegroundService.stopDownloadForegroundService(false, true);
        assertEquals(INVALID_NOTIFICATION_ID, getPinnedNotificationId());
    }
}
