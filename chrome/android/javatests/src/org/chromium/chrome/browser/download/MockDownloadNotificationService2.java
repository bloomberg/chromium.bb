// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.notifications.NotificationConstants.DEFAULT_NOTIFICATION_ID;

import android.app.Notification;
import android.content.Intent;
import android.graphics.Bitmap;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.PendingState;

import java.util.ArrayList;
import java.util.List;

/**
 * Mock class to DownloadNotificationService for testing purpose.
 */
public class MockDownloadNotificationService2 extends DownloadNotificationService2 {
    private final List<Integer> mNotificationIds = new ArrayList<Integer>();
    private boolean mPaused = false;
    private int mLastNotificationId = DEFAULT_NOTIFICATION_ID;

    List<String> mResumedDownloads = new ArrayList<>();

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

    public int getLastNotificationId() {
        return mLastNotificationId;
    }

    @Override
    public void cancelNotification(int notificationId, ContentId id) {
        super.cancelNotification(notificationId, id);
        mNotificationIds.remove(Integer.valueOf(notificationId));
    }

    @Override
    public int notifyDownloadSuccessful(final ContentId id, final String filePath,
            final String fileName, final long systemDownloadId, final boolean isOffTheRecord,
            final boolean isSupportedMimeType, final boolean isOpenable, final Bitmap icon,
            final String originalUrl, final String referrer) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> MockDownloadNotificationService2.super.notifyDownloadSuccessful(id,
                                filePath, fileName, systemDownloadId, isOffTheRecord,
                                isSupportedMimeType, isOpenable, icon, originalUrl, referrer));
    }

    @Override
    public void notifyDownloadProgress(final ContentId id, final String fileName,
            final Progress progress, final long bytesReceived, final long timeRemainingInMillis,
            final long startTime, final boolean isOffTheRecord,
            final boolean canDownloadWhileMetered, final boolean isTransient, final Bitmap icon) {
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService2.super.notifyDownloadProgress(id,
                                fileName, progress, bytesReceived, timeRemainingInMillis, startTime,
                                isOffTheRecord, canDownloadWhileMetered, isTransient, icon));
    }

    @Override
    void notifyDownloadPaused(ContentId id, String fileName, boolean isResumable,
            boolean isAutoResumable, boolean isOffTheRecord, boolean isTransient, Bitmap icon,
            boolean hasUserGesture, boolean forceRebuild, @PendingState int pendingState) {
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService2.super.notifyDownloadPaused(id, fileName,
                                isResumable, isAutoResumable, isOffTheRecord, isTransient, icon,
                                hasUserGesture, forceRebuild, pendingState));
    }

    @Override
    public void notifyDownloadFailed(final ContentId id, final String fileName, final Bitmap icon) {
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService2.super.notifyDownloadFailed(
                                id, fileName, icon));
    }

    @Override
    public void notifyDownloadCanceled(final ContentId id, boolean hasUserGesture) {
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService2.super.notifyDownloadCanceled(
                                id, hasUserGesture));
    }

    @Override
    void resumeDownload(Intent intent) {
        mResumedDownloads.add(IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_CONTENTID_ID));
    }
}
