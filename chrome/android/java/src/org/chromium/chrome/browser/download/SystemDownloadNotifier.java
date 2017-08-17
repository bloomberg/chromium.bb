// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;

import org.chromium.components.offline_items_collection.ContentId;

/**
 * DownloadNotifier implementation that creates and updates download notifications.
 * This class creates the {@link DownloadNotificationService} when needed, and binds
 * to the latter to issue calls to show and update notifications.
 */
public class SystemDownloadNotifier implements DownloadNotifier {
    private final Context mApplicationContext;
    private final DownloadNotificationService mDownloadNotificationService;

    /**
     * Constructor.
     * @param context Application context.
     */
    public SystemDownloadNotifier(Context context) {
        mApplicationContext = context.getApplicationContext();
        mDownloadNotificationService = new DownloadNotificationService();
    }

    @Override
    public void notifyDownloadCanceled(ContentId id) {
        mDownloadNotificationService.notifyDownloadCanceled(id);
    }

    @Override
    public void notifyDownloadSuccessful(DownloadInfo info, long systemDownloadId,
            boolean canResolve, boolean isSupportedMimeType) {
        final int notificationId = mDownloadNotificationService.notifyDownloadSuccessful(
                info.getContentId(), info.getFilePath(), info.getFileName(), systemDownloadId,
                info.isOffTheRecord(), isSupportedMimeType, info.getIsOpenable(), info.getIcon(),
                info.getOriginalUrl(), info.getReferrer());

        if (info.getIsOpenable()) {
            DownloadManagerService.getDownloadManagerService().onSuccessNotificationShown(
                    info, canResolve, notificationId, systemDownloadId);
        }
    }

    @Override
    public void notifyDownloadFailed(DownloadInfo info) {
        mDownloadNotificationService.notifyDownloadFailed(
                info.getContentId(), info.getFileName(), info.getIcon());
    }

    @Override
    public void notifyDownloadProgress(
            DownloadInfo info, long startTime, boolean canDownloadWhileMetered) {
        mDownloadNotificationService.notifyDownloadProgress(info.getContentId(), info.getFileName(),
                info.getProgress(), info.getBytesReceived(), info.getTimeRemainingInMillis(),
                startTime, info.isOffTheRecord(), canDownloadWhileMetered, info.getIsTransient(),
                info.getIcon());
    }

    @Override
    public void notifyDownloadPaused(DownloadInfo info) {
        mDownloadNotificationService.notifyDownloadPaused(info.getContentId(), info.getFileName(),
                true, false, info.isOffTheRecord(), info.getIsTransient(), info.getIcon());
    }

    @Override
    public void notifyDownloadInterrupted(DownloadInfo info, boolean isAutoResumable) {
        mDownloadNotificationService.notifyDownloadPaused(info.getContentId(), info.getFileName(),
                info.isResumable(), isAutoResumable, info.isOffTheRecord(), info.getIsTransient(),
                info.getIcon());
    }

    @Override
    public void removeDownloadNotification(int notificationId, DownloadInfo info) {
        mDownloadNotificationService.cancelNotification(notificationId, info.getContentId());
    }

    @Override
    public void resumePendingDownloads() {
        if (!DownloadNotificationService.isTrackingResumableDownloads(mApplicationContext)) return;
        mDownloadNotificationService.resumeAllPendingDownloads();
    }
}
