// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Notification;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.AppHooks;

/**
 * Keep-alive foreground service for downloads.
 */
public class DownloadForegroundService extends Service {
    private final IBinder mBinder = new LocalBinder();

    /**
     * Start the foreground service with this given context.
     * @param context The context used to start service.
     */
    public static void startDownloadForegroundService(Context context) {
        AppHooks.get().startForegroundService(new Intent(context, DownloadForegroundService.class));
    }

    /**
     * Update the foreground service to be pinned to a different notification.
     * @param notificationId The id of the new notification to be pinned to.
     * @param notification The new notification to be pinned to.
     */
    public void startOrUpdateForegroundService(int notificationId, Notification notification) {
        startForeground(notificationId, notification);
    }

    /**
     * Stop the foreground service that is running.
     */
    public void stopDownloadForegroundService(boolean isCancelled) {
        stopForeground(isCancelled /* kill notification if cancelled */);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // This should restart service after Chrome gets killed (except for Android 4.4.2).
        return START_STICKY;
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    /**
     * Class for clients to access.
     */
    public class LocalBinder extends Binder {
        DownloadForegroundService getService() {
            return DownloadForegroundService.this;
        }
    }
}
