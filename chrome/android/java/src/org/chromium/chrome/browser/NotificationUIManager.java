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
import android.widget.Toast;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;

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
    private static final String ACTION_SITE_SETTINGS_NOTIFICATION =
            "org.chromium.chrome.browser.ACTION_SITE_SETTINGS_NOTIFICATION";

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

        // TODO(peter): This should be handled by a broadcast receiver outside of the Notification
        // system, as it will also be used by other features.
        filter.addAction(ACTION_SITE_SETTINGS_NOTIFICATION);

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
        } else if (ACTION_SITE_SETTINGS_NOTIFICATION.equals(intent.getAction())) {
            // TODO(peter): Remove this when the intent goes elsewhere.
            Toast.makeText(mAppContext, "Not implemented.", Toast.LENGTH_SHORT).show();
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
     * @param notificationId Unique id provided by the Chrome Notification system.
     * @param title Title to be displayed in the notification.
     * @param body Message to be displayed in the notification. Will be trimmed to one line of
     *             text by the Android notification system.
     * @param icon Icon to be displayed in the notification. When this isn't a valid Bitmap, a
     *             default icon will be generated instead.
     * @param origin Full text of the origin, including the protocol, owning this notification.
     * @return The id using which the notification can be identified.
     */
    @CalledByNative
    private int displayNotification(String notificationId, String title, String body, Bitmap icon,
                                    String origin) {
        // TODO(peter): Create a default icon if |icon| is not sufficient.
        Notification notification = new Notification.Builder(mAppContext)
                .setContentTitle(title)
                .setContentText(body)
                .setStyle(new Notification.BigTextStyle().bigText(body))
                .setLargeIcon(icon)
                .setSmallIcon(android.R.drawable.ic_menu_myplaces)
                .setContentIntent(getPendingIntent(
                        notificationId, mLastNotificationId, ACTION_CLICK_NOTIFICATION))
                .setDeleteIntent(getPendingIntent(
                        notificationId, mLastNotificationId, ACTION_CLOSE_NOTIFICATION))
                .addAction(R.drawable.globe_favicon, "Site settings", getPendingIntent(
                        notificationId, mLastNotificationId, ACTION_SITE_SETTINGS_NOTIFICATION))
                .setSubText(origin)
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
