// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.test.ServiceTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;

import java.util.HashSet;
import java.util.Set;

/**
 * Tests of {@link DownloadNotificationService}.
 */
public class DownloadNotificationServiceTest extends
        ServiceTestCase<MockDownloadNotificationService> {

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
        notifications.add(new DownloadManagerService.PendingNotification(
                1, "test1", true).getNotificationString());
        notifications.add(new DownloadManagerService.PendingNotification(
                2, "test2", true).getNotificationString());
        SharedPreferences sharedPrefs =
                PreferenceManager.getDefaultSharedPreferences(mockContext);
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putStringSet(DownloadManagerService.PENDING_DOWNLOAD_NOTIFICATIONS, notifications);
        editor.apply();
        startNotificationService();
        assertTrue(getService().isPaused());
        assertEquals(2, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(1));
        assertTrue(getService().getNotificationIds().contains(2));
    }

    /**
     * Tests adding and cancelling notifications.
     */
    @SmallTest
    @Feature({"Download"})
    public void testAddingAndCancelingNotifications() {
        setupService();
        startNotificationService();
        DownloadNotificationService service = bindNotificationService();
        service.notifyDownloadProgress(1, "test", -1, 1L, 1L, true);
        assertEquals(1, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(1));

        service.notifyDownloadSuccessful(2, "test2", null);
        assertEquals(2, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(2));

        service.notifyDownloadFailed(3, "test3");
        assertEquals(3, getService().getNotificationIds().size());
        assertTrue(getService().getNotificationIds().contains(3));

        service.cancelNotification(1);
        assertEquals(2, getService().getNotificationIds().size());
        assertFalse(!getService().getNotificationIds().contains(1));
    }
}
