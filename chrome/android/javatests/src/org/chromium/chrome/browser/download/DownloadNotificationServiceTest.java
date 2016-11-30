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
import android.test.ServiceTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;

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
        public void resumeDownload(DownloadItem item, boolean hasUserGesture) {
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

    public DownloadNotificationServiceTest() {
        super(MockDownloadNotificationService.class);
    }

    @Override
    protected void setupService() {
        super.setupService();
    }

    @Override
    protected void tearDown() throws Exception {
        super.setupService();
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.remove(DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS);
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
        notifications.add(
                new DownloadSharedPreferenceEntry(1, false, true, UUID.randomUUID().toString(),
                        "test1", DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
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
        notifications.add(
                new DownloadSharedPreferenceEntry(1, false, true, UUID.randomUUID().toString(),
                        "test1", DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, false)
                        .getSharedPreferenceString());
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
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
        notifications.add(
                new DownloadSharedPreferenceEntry(1, false, true, UUID.randomUUID().toString(),
                        "test1", DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        notifications.add(
                new DownloadSharedPreferenceEntry(2, false, true, UUID.randomUUID().toString(),
                        "test2", DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        SharedPreferences sharedPrefs =
                ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        assertTrue(getService().isPaused());
        assertEquals(2, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(1));
        assertTrue(getService().getNotificationIds().contains(2));
        assertTrue(
                sharedPrefs.contains(DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS));
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
        notifications.add(new DownloadSharedPreferenceEntry(3, false, true, guid1, "success",
                DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        String guid2 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(4, false, true, guid2, "failed",
                DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        assertEquals(2, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(3));
        assertTrue(getService().getNotificationIds().contains(4));

        DownloadNotificationService service = bindNotificationService();
        String guid3 = UUID.randomUUID().toString();
        service.notifyDownloadProgress(guid3, "test", 1, 1L, 1L, true, true, false);
        assertEquals(3, getService().getNotificationIds().size());
        int lastNotificationId = getService().getLastAddedNotificationId();
        Set<String> entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(3, entries.size());

        service.notifyDownloadSuccessful(guid1, "/path/to/success", "success", 100L, false, false);
        entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(2, entries.size());

        service.notifyDownloadFailed(guid2, "failed");
        entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(1, entries.size());

        service.notifyDownloadCanceled(guid3);
        assertEquals(2, getService().getNotificationIds().size());
        assertFalse(getService().getNotificationIds().contains(lastNotificationId));
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
        String guid = UUID.randomUUID().toString();
        service.notifyDownloadSuccessful(guid, "/path/to/test", "test", 100L, false, false);
        assertEquals(1, getService().getNotificationIds().size());
    }

    /**
     * Tests resume all pending downloads. Only auto resumable downloads can resume.
     */
    @SmallTest
    @Feature({"Download"})
    public void testResumeAllPendingDownloads() throws Exception {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<>();
        String guid1 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(3, false, false, guid1, "success",
                DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        String guid2 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(4, false, true, guid2, "failed",
                DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        String guid3 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(5, false, true, guid3, "nonresumable",
                DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, false)
                        .getSharedPreferenceString());
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
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
    public void testIncognitoDownloadCanceledOnBrowserKill() throws Exception {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<>();
        String uuid = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(1, true, true, uuid, "test1",
                DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD, true)
                        .getSharedPreferenceString());
        SharedPreferences sharedPrefs =
                ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(
                DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        assertTrue(getService().isPaused());
        assertFalse(sharedPrefs.contains(
                DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS));
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
}
