// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;

import org.chromium.base.CalledByNative;

/**
 * Provides the ability for the NotificationUIManagerAndroid to talk to the Android platform
 * notification manager.
 *
 * This class should only be used on the UI thread.
 */
public class NotificationUIManager extends BroadcastReceiver {
    private static final String EXTRA_NOTIFICATION_ID = "notification_id";

    private static final String ACTION_CLICK_NOTIFICATION =
            "org.chromium.chrome.browser.CLICK_NOTIFICATION";
    private static final String ACTION_CLOSE_NOTIFICATION =
            "org.chromium.chrome.browser.CLOSE_NOTIFICATION";

    private final long mNativeNotificationManager;

    private final Context mAppContext;
    private final NotificationManager mNotificationManager;

    private int mLastNotificationId;

    private NotificationUIManager(long nativeNotificationManager, Context context) {
        super();

        mNativeNotificationManager = nativeNotificationManager;
        mAppContext = context.getApplicationContext();

        mNotificationManager = (NotificationManager)
                mAppContext.getSystemService(Context.NOTIFICATION_SERVICE);

        mLastNotificationId = 0;

        // Register this instance as a broadcast receiver, which will be used for handling
        // clicked-on and rejected notifications.
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_CLICK_NOTIFICATION);
        filter.addAction(ACTION_CLOSE_NOTIFICATION);

        // TODO(peter): Notification events should be received by an intent receiver that does not
        // require Chrome to be running all the time.
        mAppContext.registerReceiver(this, filter);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!intent.hasExtra(EXTRA_NOTIFICATION_ID))
            return;

        String notificationId = intent.getStringExtra(EXTRA_NOTIFICATION_ID);
        if (ACTION_CLICK_NOTIFICATION.equals(intent.getAction())) {
            nativeOnNotificationClicked(mNativeNotificationManager, notificationId);
        } else if (ACTION_CLOSE_NOTIFICATION.equals(intent.getAction())) {
            nativeOnNotificationClosed(mNativeNotificationManager, notificationId);
        }
    }

    private PendingIntent getPendingIntent(String notificationId, int platformId, String action) {
        Intent intent = new Intent(action);
        intent.putExtra(EXTRA_NOTIFICATION_ID, notificationId);

        return PendingIntent.getBroadcast(mAppContext, platformId, intent, 0);
    }

    /**
     * Displays a notification with the given |notificationId|, |title|, |body| and |icon|.
     *
     * @returns The id using which the notification can be identified.
     */
    @CalledByNative
    private int displayNotification(String notificationId, String title, String body, Bitmap icon) {
        Notification notification = new Notification.Builder(mAppContext)
                .setContentTitle(title)
                .setContentText(body)
                .setLargeIcon(icon)
                .setSmallIcon(android.R.drawable.ic_menu_myplaces)
                .setContentIntent(getPendingIntent(
                        notificationId, mLastNotificationId, ACTION_CLICK_NOTIFICATION))
                .setDeleteIntent(getPendingIntent(
                        notificationId, mLastNotificationId, ACTION_CLOSE_NOTIFICATION))
                .build();

        mNotificationManager.notify(mLastNotificationId, notification);

        return mLastNotificationId++;
    }

    /**
     * Closes the notification identified by |platformId|.
     */
    @CalledByNative
    private void closeNotification(int platformId) {
        mNotificationManager.cancel(platformId);
    }

    @CalledByNative
    private static NotificationUIManager create(long nativeNotificationManager, Context context) {
        return new NotificationUIManager(nativeNotificationManager, context);
    }

    private native void nativeOnNotificationClicked(
            long nativeNotificationUIManagerAndroid, String notificationId);

    private native void nativeOnNotificationClosed(
            long nativeNotificationUIManagerAndroid, String notificationId);
}
