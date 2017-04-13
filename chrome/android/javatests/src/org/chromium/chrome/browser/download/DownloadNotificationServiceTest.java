// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.support.test.filters.SmallTest;
import android.test.ServiceTestCase;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

/**
 * Tests of {@link DownloadNotificationService}.
 */
public class DownloadNotificationServiceTest extends
        ServiceTestCase<MockDownloadNotificationService> {
    private static final int MILLIS_PER_SECOND = 1000;

    private static class MockDownloadManagerService extends DownloadManagerService {
        final List<DownloadItem> mDownloads = new ArrayList<DownloadItem>();

        public MockDownloadManagerService(Context context) {
            super(context, null, getTestHandler(), 1000);
        }

        @Override
        protected void init() {}

        @Override
        public void resumeDownload(ContentId id, DownloadItem item, boolean hasUserGesture) {
            mDownloads.add(item);
        }
    }

    private static class MockDownloadResumptionScheduler extends DownloadResumptionScheduler {
        boolean mScheduled;

        public MockDownloadResumptionScheduler(Context context) {
            super(context);
        }

        @Override
        public void schedule(boolean allowMeteredConnection) {
            mScheduled = true;
        }

        @Override
        public void cancelTask() {
            mScheduled = false;
        }
    }

    private static String buildEntryStringWithGuid(String guid, int notificationId, String fileName,
            boolean metered, boolean autoResume, boolean offTheRecord) {
        return new DownloadSharedPreferenceEntry(LegacyHelpers.buildLegacyContentId(false, guid),
                notificationId, offTheRecord, metered, fileName, autoResume, false)
                .getSharedPreferenceString();
    }

    private static String buildEntryStringWithGuid(
            String guid, int notificationId, String fileName, boolean metered, boolean autoResume) {
        return buildEntryStringWithGuid(guid, notificationId, fileName, metered, autoResume, false);
    }

    private static String buildEntryString(
            int notificationId, String fileName, boolean metered, boolean autoResume) {
        return buildEntryStringWithGuid(
                UUID.randomUUID().toString(), notificationId, fileName, metered, autoResume);
    }

    public DownloadNotificationServiceTest() {
        super(MockDownloadNotificationService.class);
    }

    @Override
    protected void setupService() {
        super.setupService();
    }

    @Override
    protected void shutdownService() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                DownloadNotificationServiceTest.super.shutdownService();
            }
        });
    }

    @Override
    protected void tearDown() throws Exception {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.remove(DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS);
        editor.apply();
        super.tearDown();
    }

    private void startNotificationService() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(getService(), MockDownloadNotificationService.class);
                startService(intent);
            }
        });
    }

    private DownloadNotificationService bindNotificationService() {
        Intent intent = new Intent(getService(), MockDownloadNotificationService.class);
        IBinder service = bindService(intent);
        return ((DownloadNotificationService.LocalBinder) service).getService();
    }

    private static Handler getTestHandler() {
        HandlerThread handlerThread = new HandlerThread("handlerThread");
        handlerThread.start();
        return new Handler(handlerThread.getLooper());
    }

    private void resumeAllDownloads(final DownloadNotificationService service) throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                service.resumeAllPendingDownloads();
            }
        });
    }

    /**
     * Tests that creating the service without launching chrome will do nothing if there is no
     * ongoing download.
     */
    @SmallTest
    @Feature({"Download"})
    public void testPausingWithoutOngoingDownloads() {
        setupService();
        startNotificationService();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getService().updateNotificationsForShutdown();
            }
        });
        assertTrue(getService().isPaused());
        assertTrue(getService().getNotificationIds().isEmpty());
    }

    /**
     * Tests that download resumption task is scheduled when notification service is started
     * without any download action.
     */
    @SmallTest
    @Feature({"Download"})
    public void testResumptionScheduledWithoutDownloadOperationIntent() throws Exception {
        MockDownloadResumptionScheduler scheduler = new MockDownloadResumptionScheduler(
                getSystemContext().getApplicationContext());
        DownloadResumptionScheduler.setDownloadResumptionScheduler(scheduler);
        setupService();
        Set<String> notifications = new HashSet<>();
        notifications.add(buildEntryString(1, "test1", true, true));
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        shutdownService();
        assertTrue(scheduler.mScheduled);
    }

    /**
     * Tests that download resumption task is not scheduled when notification service is started
     * with a download action.
     */
    @SmallTest
    @Feature({"Download"})
    public void testResumptionNotScheduledWithDownloadOperationIntent() {
        MockDownloadResumptionScheduler scheduler = new MockDownloadResumptionScheduler(
                getSystemContext().getApplicationContext());
        DownloadResumptionScheduler.setDownloadResumptionScheduler(scheduler);
        setupService();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(getService(), MockDownloadNotificationService.class);
                intent.setAction(DownloadNotificationService.ACTION_DOWNLOAD_RESUME_ALL);
                startService(intent);
            }
        });
        assertFalse(scheduler.mScheduled);
    }

    /**
     * Tests that download resumption task is not scheduled when there is no auto resumable
     * download in SharedPreferences.
     */
    @SmallTest
    @Feature({"Download"})
    public void testResumptionNotScheduledWithoutAutoResumableDownload() throws Exception {
        MockDownloadResumptionScheduler scheduler = new MockDownloadResumptionScheduler(
                getSystemContext().getApplicationContext());
        DownloadResumptionScheduler.setDownloadResumptionScheduler(scheduler);
        setupService();
        Set<String> notifications = new HashSet<>();
        notifications.add(buildEntryString(1, "test1", true, false));
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        assertFalse(scheduler.mScheduled);
    }

    /**
     * Tests that creating the service without launching chrome will pause all ongoing downloads.
     */
    @SmallTest
    @Feature({"Download"})
    public void testPausingWithOngoingDownloads() {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<>();
        notifications.add(buildEntryString(1, "test1", true, true));
        notifications.add(buildEntryString(2, "test2", true, true));
        SharedPreferences sharedPrefs =
                ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getService().updateNotificationsForShutdown();
            }
        });
        assertTrue(getService().isPaused());
        assertEquals(2, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(1));
        assertTrue(getService().getNotificationIds().contains(2));
        assertTrue(sharedPrefs.contains(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS));
    }

    /**
     * Tests adding and cancelling notifications.
     */
    @SmallTest
    @Feature({"Download"})
    public void testAddingAndCancelingNotifications() {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<>();
        String guid1 = UUID.randomUUID().toString();
        String guid2 = UUID.randomUUID().toString();
        notifications.add(buildEntryStringWithGuid(guid1, 3, "success", true, true));
        notifications.add(buildEntryStringWithGuid(guid2, 4, "failed", true, true));
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getService().updateNotificationsForShutdown();
            }
        });
        assertEquals(2, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(3));
        assertTrue(getService().getNotificationIds().contains(4));

        DownloadNotificationService service = bindNotificationService();
        ContentId id3 = LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());
        service.notifyDownloadProgress(id3, "test", 1, 100L, 1L, 1L, true, true, false);
        assertEquals(3, getService().getNotificationIds().size());
        int lastNotificationId = getService().getLastAddedNotificationId();
        Set<String> entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(3, entries.size());

        ContentId id1 = LegacyHelpers.buildLegacyContentId(false, guid1);
        service.notifyDownloadSuccessful(
                id1, "/path/to/success", "success", 100L, false, false, true);
        entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(2, entries.size());

        ContentId id2 = LegacyHelpers.buildLegacyContentId(false, guid2);
        service.notifyDownloadFailed(id2, "failed");
        entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(1, entries.size());

        service.notifyDownloadCanceled(id3);
        assertEquals(2, getService().getNotificationIds().size());
        assertFalse(getService().getNotificationIds().contains(lastNotificationId));

        ContentId id4 = LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());
        service.notifyDownloadSuccessful(
                id4, "/path/to/success", "success", 100L, false, false, true);
        assertEquals(3, getService().getNotificationIds().size());
        int nextNotificationId = getService().getLastAddedNotificationId();
        service.cancelNotification(nextNotificationId, id4);
        assertEquals(2, getService().getNotificationIds().size());
        assertFalse(getService().getNotificationIds().contains(nextNotificationId));
    }

    /**
     * Tests that notification is updated if download success comes without any prior progress.
     */
    @SmallTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testDownloadSuccessNotification() {
        setupService();
        startNotificationService();
        DownloadNotificationService service = bindNotificationService();
        ContentId id = LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());
        service.notifyDownloadSuccessful(id, "/path/to/test", "test", 100L, false, false, true);
        assertEquals(1, getService().getNotificationIds().size());
    }

    /**
     * Tests resume all pending downloads. Only auto resumable downloads can resume.
     */
    @SmallTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testResumeAllPendingDownloads() throws Exception {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<>();
        String guid1 = UUID.randomUUID().toString();
        String guid2 = UUID.randomUUID().toString();
        String guid3 = UUID.randomUUID().toString();

        notifications.add(buildEntryStringWithGuid(guid1, 3, "success", false, true));
        notifications.add(buildEntryStringWithGuid(guid2, 4, "failed", true, true));
        notifications.add(buildEntryStringWithGuid(guid3, 5, "nonresumable", true, false));

        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        DownloadNotificationService service = bindNotificationService();
        DownloadManagerService.disableNetworkListenerForTest();

        final MockDownloadManagerService manager =
                new MockDownloadManagerService(getSystemContext().getApplicationContext());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                DownloadManagerService.setDownloadManagerService(manager);
            }
        });
        DownloadManagerService.setIsNetworkMeteredForTest(true);
        resumeAllDownloads(service);
        assertEquals(1, manager.mDownloads.size());
        assertEquals(manager.mDownloads.get(0).getDownloadInfo().getDownloadGuid(), guid2);

        manager.mDownloads.clear();
        DownloadManagerService.setIsNetworkMeteredForTest(false);
        resumeAllDownloads(service);
        assertEquals(1, manager.mDownloads.size());
        assertEquals(manager.mDownloads.get(0).getDownloadInfo().getDownloadGuid(), guid1);
    }

    /**
     * Tests incognito download fails when browser gets killed.
     */
    @SmallTest
    @Feature({"Download"})
    public void testIncognitoDownloadCanceledOnServiceShutdown() throws Exception {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<>();
        String uuid = UUID.randomUUID().toString();
        notifications.add(buildEntryStringWithGuid(uuid, 1, "test1", true, true, true));
        SharedPreferences sharedPrefs =
                ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getService().onTaskRemoved(new Intent());
            }
        });

        assertTrue(getService().isPaused());
        assertFalse(sharedPrefs.contains(
                DownloadSharedPreferenceHelper.KEY_PENDING_DOWNLOAD_NOTIFICATIONS));
    }

    @SmallTest
    @Feature({"Download"})
    public void testFormatRemainingTime() {
        Context context = getSystemContext().getApplicationContext();
        assertEquals("0 secs left", DownloadNotificationService.formatRemainingTime(context, 0));
        assertEquals("1 sec left", DownloadNotificationService.formatRemainingTime(
                context, MILLIS_PER_SECOND));
        assertEquals("1 min left", DownloadNotificationService.formatRemainingTime(context,
                DownloadNotificationService.SECONDS_PER_MINUTE * MILLIS_PER_SECOND));
        assertEquals("2 mins left", DownloadNotificationService.formatRemainingTime(context,
                149 * MILLIS_PER_SECOND));
        assertEquals("3 mins left", DownloadNotificationService.formatRemainingTime(context,
                150 * MILLIS_PER_SECOND));
        assertEquals("1 hour left", DownloadNotificationService.formatRemainingTime(context,
                DownloadNotificationService.SECONDS_PER_HOUR * MILLIS_PER_SECOND));
        assertEquals("2 hours left", DownloadNotificationService.formatRemainingTime(context,
                149 * DownloadNotificationService.SECONDS_PER_MINUTE * MILLIS_PER_SECOND));
        assertEquals("3 hours left", DownloadNotificationService.formatRemainingTime(context,
                150 * DownloadNotificationService.SECONDS_PER_MINUTE * MILLIS_PER_SECOND));
        assertEquals("1 day left", DownloadNotificationService.formatRemainingTime(context,
                DownloadNotificationService.SECONDS_PER_DAY * MILLIS_PER_SECOND));
        assertEquals("2 days left", DownloadNotificationService.formatRemainingTime(context,
                59 * DownloadNotificationService.SECONDS_PER_HOUR * MILLIS_PER_SECOND));
        assertEquals("3 days left", DownloadNotificationService.formatRemainingTime(context,
                60 * DownloadNotificationService.SECONDS_PER_HOUR * MILLIS_PER_SECOND));
    }

    // Tests that the downloaded bytes on the notification is correct.
    @SmallTest
    @Feature({"Download"})
    public void testFormatBytesReceived() {
        Context context = getSystemContext().getApplicationContext();
        assertEquals("Downloaded 0.0 KB", DownloadUtils.getStringForBytes(
                context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 0));
        assertEquals("Downloaded 0.5 KB", DownloadUtils.getStringForBytes(
                context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 512));
        assertEquals("Downloaded 1.0 KB", DownloadUtils.getStringForBytes(
                context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 1024));
        assertEquals("Downloaded 1.0 MB", DownloadUtils.getStringForBytes(
                context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 1024 * 1024));
        assertEquals("Downloaded 1.0 GB", DownloadUtils.getStringForBytes(
                context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 1024 * 1024 * 1024));
    }
}
