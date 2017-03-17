// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Notification;
import android.content.Context;
import android.util.Pair;

import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Mock class to DownloadNotificationService for testing purpose.
 */
public class MockDownloadNotificationService extends DownloadNotificationService {
    private final List<Integer> mNotificationIds = new ArrayList<Integer>();
    private boolean mPaused = false;
    private Context mContext;
    private int mLastNotificationId;

    void setContext(Context context) {
        mContext = context;
    }

    @Override
    public void stopForegroundInternal(boolean killNotification) {
        if (!useForegroundService()) return;
        if (killNotification) mNotificationIds.clear();
    }

    @Override
    public void startForegroundInternal() {}

    @Override
    public void cancelOffTheRecordDownloads() {
        super.cancelOffTheRecordDownloads();
        mPaused = true;
    }

    @Override
    boolean hasDownloadNotificationsInternal(int notificationIdToIgnore) {
        if (!useForegroundService()) return false;
        // Cancelling notifications here is synchronous, so we don't really have to worry about
        // {@code notificationIdToIgnore}, but address it properly anyway.
        if (mNotificationIds.size() == 1 && notificationIdToIgnore != -1) {
            return !mNotificationIds.contains(notificationIdToIgnore);
        }

        return !mNotificationIds.isEmpty();
    }

    @Override
    void updateSummaryIconInternal(
            int removedNotificationId, Pair<Integer, Notification> addedNotification) {}

    @Override
    void cancelSummaryNotification() {}

    @Override
    void updateNotification(int id, Notification notification) {
        if (!mNotificationIds.contains(id)) {
            mNotificationIds.add(id);
            mLastNotificationId = id;
        }
    }

    public boolean isPaused() {
        return mPaused;
    }

    public List<Integer> getNotificationIds() {
        return mNotificationIds;
    }

    @Override
    public void cancelNotification(int notificationId, String downloadGuid) {
        super.cancelNotification(notificationId, downloadGuid);
        mNotificationIds.remove(Integer.valueOf(notificationId));
    }

    public int getLastAddedNotificationId() {
        return mLastNotificationId;
    }

    @Override
    public Context getApplicationContext() {
        return mContext == null ? super.getApplicationContext() : mContext;
    }

    @Override
    public int notifyDownloadSuccessful(final String downloadGuid, final String filePath,
            final String fileName, final long systemDownloadId, final boolean isOffTheRecord,
            final boolean isOfflinePage, final boolean isSupportedMimeType) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return MockDownloadNotificationService.super.notifyDownloadSuccessful(downloadGuid,
                        filePath, fileName, systemDownloadId, isOffTheRecord, isOfflinePage,
                        isSupportedMimeType);
            }
        });
    }

    @Override
    public void notifyDownloadProgress(final String downloadGuid, final String fileName,
            final int percentage, final long bytesReceived, final long timeRemainingInMillis,
            final long startTime, final boolean isOffTheRecord,
            final boolean canDownloadWhileMetered, final boolean isOfflinePage) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                MockDownloadNotificationService.super.notifyDownloadProgress(
                        downloadGuid, fileName, percentage, bytesReceived, timeRemainingInMillis,
                        startTime, isOffTheRecord, canDownloadWhileMetered, isOfflinePage);
            }
        });
    }

    @Override
    public void notifyDownloadFailed(
            final boolean isOfflinePage, final String downloadGuid, final String fileName) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                MockDownloadNotificationService.super.notifyDownloadFailed(
                        isOfflinePage, downloadGuid, fileName);
            }
        });
    }

    @Override
    public void notifyDownloadCanceled(final String downloadGuid) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                MockDownloadNotificationService.super.notifyDownloadCanceled(downloadGuid);
            }
        });
    }
}

