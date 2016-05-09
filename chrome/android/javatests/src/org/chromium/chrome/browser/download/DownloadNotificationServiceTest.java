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
    private static class MockDownloadManagerService extends DownloadManagerService {
        final List<DownloadItem> mDownloads = new ArrayList<DownloadItem>();

        public MockDownloadManagerService(Context context) {
            super(context, null, getTestHandler(), 1000);
        }

        @Override
        protected void init() {}

        @Override
        protected void resumeDownload(DownloadItem item, boolean hasUserGesture) {
            mDownloads.add(item);
        }
    }

    public DownloadNotificationServiceTest() {
        super(MockDownloadNotificationService.class);
    }

    @Override
    protected void setupService() {
        super.setupService();
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
     * Tests that creating the service without launching chrome will pause all ongoing downloads.
     */
    @SmallTest
    @Feature({"Download"})
    public void testPausingWithOngoingDownloads() {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<String>();
        notifications.add(new DownloadSharedPreferenceEntry(1, true, true,
                UUID.randomUUID().toString(), "test1").getSharedPreferenceString());
        notifications.add(new DownloadSharedPreferenceEntry(2, true, true,
                UUID.randomUUID().toString(), "test2").getSharedPreferenceString());
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
        Set<String> notifications = new HashSet<String>();
        String guid1 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(3, true, true, guid1, "success")
                .getSharedPreferenceString());
        String guid2 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(4, true, true, guid2, "failed")
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
        service.notifyDownloadProgress(1, guid3, "test", 1, 1L, 1L, true, true);
        assertEquals(3, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(1));
        Set<String> entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(3, entries.size());

        service.notifyDownloadSuccessful(guid1, null);
        entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(2, entries.size());

        service.notifyDownloadFailed(guid2);
        entries = DownloadManagerService.getStoredDownloadInfo(
                sharedPrefs, DownloadNotificationService.PENDING_DOWNLOAD_NOTIFICATIONS);
        assertEquals(1, entries.size());

        service.cancelNotification(1, guid3);
        assertEquals(2, getService().getNotificationIds().size());
        assertFalse(getService().getNotificationIds().contains(1));
    }

    /**
     * Tests resume all pending downloads.
     */
    @SmallTest
    @Feature({"Download"})
    public void testResumeAllPendingDownloads() throws Exception {
        setupService();
        Context mockContext = new AdvancedMockContext(getSystemContext());
        getService().setContext(mockContext);
        Set<String> notifications = new HashSet<String>();
        String guid1 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(3, true, false, guid1, "success")
                .getSharedPreferenceString());
        String guid2 = UUID.randomUUID().toString();
        notifications.add(new DownloadSharedPreferenceEntry(4, true, true, guid2, "failed")
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
        assertEquals(2, manager.mDownloads.size());
    }

    @SmallTest
    @Feature({"Download"})
    public void testParseDownloadNotifications() {
        String uuid = UUID.randomUUID().toString();
        String notification =
                DownloadSharedPreferenceEntry.VERSION + ",1,0,1," + uuid + ",test.pdf";
        DownloadSharedPreferenceEntry entry =
                DownloadSharedPreferenceEntry.parseFromString(notification);
        assertEquals(1, entry.notificationId);
        assertEquals("test.pdf", entry.fileName);
        assertFalse(entry.isResumable);
        assertEquals(uuid, entry.downloadGuid);

        notification = DownloadSharedPreferenceEntry.VERSION + ",2,1,1," + uuid + ",test,2.pdf";
        entry = DownloadSharedPreferenceEntry.parseFromString(notification);
        assertEquals(2, entry.notificationId);
        assertEquals("test,2.pdf", entry.fileName);
        assertTrue(entry.isResumable);
        assertEquals(uuid, entry.downloadGuid);
    }
}
