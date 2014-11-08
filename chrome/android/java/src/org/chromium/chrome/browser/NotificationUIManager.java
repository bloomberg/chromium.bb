// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.base.CalledByNative;

/**
 * Provides the ability for the NotificationUIManagerAndroid to talk to the Android platform
 * notification manager.
 *
 * This class should only be used on the UI thread.
 */
public class NotificationUIManager {
    private final long mNativeNotificationManager;

    private final Context mAppContext;
    private final NotificationManager mNotificationManager;

    private int mLastNotificationId;

    private NotificationUIManager(long nativeNotificationManager, Context context) {
        mNativeNotificationManager = nativeNotificationManager;
        mAppContext = context.getApplicationContext();

        mNotificationManager = (NotificationManager)
                mAppContext.getSystemService(Context.NOTIFICATION_SERVICE);

        mLastNotificationId = 0;
    }

    /**
     * Displays a notification with the given |title|, |body| and |icon|.
     *
     * @returns The id using which the notification can be identified.
     */
    @CalledByNative
    private int displayNotification(String title, String body, Bitmap icon) {
        Notification notification = new Notification.Builder(mAppContext)
                .setContentTitle(title)
                .setContentText(body)
                .setLargeIcon(icon)
                .build();
        mNotificationManager.notify(mLastNotificationId, notification);

        return mLastNotificationId++;
    }

    /**
     * Closes the notification identified by |notificationId|.
     */
    @CalledByNative
    private void closeNotification(int notificationId) {
        mNotificationManager.cancel(notificationId);
    }

    @CalledByNative
    private static NotificationUIManager create(long nativeNotificationManager, Context context) {
        return new NotificationUIManager(nativeNotificationManager, context);
    }
}
