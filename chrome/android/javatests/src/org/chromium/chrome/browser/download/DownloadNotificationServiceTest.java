// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.SharedPreferences;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;

import java.util.UUID;

/**
 * Tests of {@link DownloadNotificationService}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DownloadNotificationServiceTest {
    private static final ContentId ID1 =
            LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());
    private static final ContentId ID2 =
            LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());
    private static final ContentId ID3 =
            LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());

    private MockDownloadNotificationService mDownloadNotificationService;
    private DownloadForegroundServiceManagerTest
            .MockDownloadForegroundServiceManager mDownloadForegroundServiceManager;
    private DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper;

    private static DownloadSharedPreferenceEntry buildEntryStringWithGuid(ContentId contentId,
            int notificationId, String fileName, boolean metered, boolean autoResume) {
        return new DownloadSharedPreferenceEntry(
                contentId, notificationId, false, metered, fileName, autoResume, false);
    }

    @Before
    public void setUp() {
        mDownloadNotificationService = new MockDownloadNotificationService();
        mDownloadForegroundServiceManager =
                new DownloadForegroundServiceManagerTest.MockDownloadForegroundServiceManager();
        mDownloadNotificationService.setDownloadForegroundServiceManager(
                mDownloadForegroundServiceManager);
        mDownloadSharedPreferenceHelper = DownloadSharedPreferenceHelper.getInstance();
    }

    @After
    public void tearDown() {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.remove(DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS);
        editor.apply();
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testBasicDownloadFlow() {
        // Download is in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(1, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L, true, true,
                false, null);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        int notificationId1 = mDownloadNotificationService.getLastNotificationId();
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is paused.
        mDownloadNotificationService.notifyDownloadPaused(
                ID1, "test", true /* isResumable*/, false /* isAutoResumable */, true, false, null);

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is again in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(20, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L, true,
                true, false, null);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is successful.
        mDownloadNotificationService.notifyDownloadSuccessful(
                ID1, "", "test", 1L, true, true, true, null, "", "");
        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testDownloadPendingAndCancelled() {
        // Download is in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(1, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L, true, true,
                false, null);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        int notificationId1 = mDownloadNotificationService.getLastNotificationId();
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is interrupted and now is pending.
        mDownloadNotificationService.notifyDownloadPaused(
                ID1, "test", true /* isResumable */, true /* isAutoResumable */, true, false, null);
        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is cancelled.
        mDownloadNotificationService.notifyDownloadCanceled(ID1);

        assertEquals(0, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testDownloadInterruptedAndFailed() {
        // Download is in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(1, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L, true, true,
                false, null);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        int notificationId1 = mDownloadNotificationService.getLastNotificationId();
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is interrupted but because it is not resumable, fails.
        mDownloadNotificationService.notifyDownloadPaused(
                ID1, "test", false /* isResumable*/, true /* isAutoResumable */, true, false, null);
        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testResumeAllPendingDownloads() {
        // Queue a few pending downloads.
        mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                buildEntryStringWithGuid(ID1, 3, "success", false, true));
        mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                buildEntryStringWithGuid(ID2, 4, "failed", true, true));
        mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                buildEntryStringWithGuid(ID3, 5, "nonresumable", true, false));

        // Resume pending downloads when network is metered.
        DownloadManagerService.disableNetworkListenerForTest();
        DownloadManagerService.setIsNetworkMeteredForTest(true);
        mDownloadNotificationService.resumeAllPendingDownloads();

        assertEquals(1, mDownloadNotificationService.mResumedDownloads.size());
        assertEquals(ID2.id, mDownloadNotificationService.mResumedDownloads.get(0));

        // Resume pending downloads when network is not metered.
        // TODO(jming): Right now, assuming all downloads are resumed because of the way downloads
        // in progress are tracked internally. Change when this is updated. http://crbug.com/755588.
        mDownloadNotificationService.mResumedDownloads.clear();
        DownloadManagerService.setIsNetworkMeteredForTest(false);
        mDownloadNotificationService.resumeAllPendingDownloads();
        assertEquals(2, mDownloadNotificationService.mResumedDownloads.size());

        mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(ID1);
        mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(ID2);
        mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(ID3);
    }
}
