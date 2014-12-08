// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

/**
 * Provides the ability for the NotificationUIManagerAndroid to talk to the Android platform
 * notification manager.
 *
 * This class should only be used on the UI thread.
 */
public class NotificationUIManager {
    private static final String TAG = NotificationUIManager.class.getSimpleName();

    private static final int NOTIFICATION_ICON_BG_COLOR = Color.rgb(150, 150, 150);
    private static final int NOTIFICATION_TEXT_SIZE_DP = 28;

    /**
     * The application has the ability to observe the UI manager by providing an Observer.
     */
    public interface Observer {
        /**
         * Will be called right before a notification is being displayed. The implementation may
         * modify the Notification's builder.
         *
         * @param notificationBuilder The NotificationBuilder which is about to be shown.
         * @param origin The origin which is displaying the notification.
         */
        void onBeforeDisplayNotification(Notification.Builder notificationBuilder,
                                         String origin);
    }

    private static NotificationUIManager sInstance;
    private static Observer sObserver;

    private final long mNativeNotificationManager;

    private final Context mAppContext;
    private final NotificationManager mNotificationManager;

    private RoundedIconGenerator mIconGenerator;

    private int mLastNotificationId;

    /**
     * Creates a new instance of the NotificationUIManager.
     *
     * @param nativeNotificationManager Instance of the NotificationUIManagerAndroid class.
     * @param context Application context for this instance of Chrome.
     */
    @CalledByNative
    private static NotificationUIManager create(long nativeNotificationManager, Context context) {
        if (sInstance != null) {
            throw new IllegalStateException("There must only be a single NotificationUIManager.");
        }

        sInstance = new NotificationUIManager(nativeNotificationManager, context);
        return sInstance;
    }

    /**
     * Sets the optional observer to be used when displaying notifications. May be NULL.
     *
     * @param observer The observer to call when displaying notifications.
     */
    public static void setObserver(Observer observer) {
        if (sObserver != null && observer != null) {
            throw new IllegalStateException("There must only be one active Observer at a time.");
        }

        sObserver = observer;
    }

    private NotificationUIManager(long nativeNotificationManager, Context context) {
        mNativeNotificationManager = nativeNotificationManager;
        mAppContext = context.getApplicationContext();

        mNotificationManager = (NotificationManager)
                mAppContext.getSystemService(Context.NOTIFICATION_SERVICE);

        mLastNotificationId = 0;
    }

    /**
     * Marks the current instance as being freed, allowing for a new NotificationUIManager
     * object to be initialized.
     */
    @CalledByNative
    private void destroy() {
        assert sInstance == this;
        sInstance = null;
    }

    /**
     * Invoked by the NotificationService when a Notification intent has been received. There may
     * not be an active instance of the NotificationUIManager at this time, so inform the native
     * side through a static method, initialzing the manager if needed.
     *
     * @param intent The intent as received by the Notification service.
     * @return Whether the event could be handled by the native Notification manager.
     */
    public static boolean dispatchNotificationEvent(Intent intent) {
        if (sInstance == null) {
            nativeInitializeNotificationUIManager();
            if (sInstance == null) {
                Log.e(TAG, "Unable to initialize the native NotificationUIManager.");
                return false;
            }
        }

        String notificationId = intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_ID);
        if (NotificationConstants.ACTION_CLICK_NOTIFICATION.equals(intent.getAction())) {
            return sInstance.onNotificationClicked(notificationId);
        } else if (NotificationConstants.ACTION_CLOSE_NOTIFICATION.equals(intent.getAction())) {
            return sInstance.onNotificationClosed(notificationId);
        } else {
            Log.e(TAG, "Unrecognized Notification action: " + intent.getAction());
            return false;
        }
    }

    private PendingIntent getPendingIntent(String notificationId, int platformId, String action) {
        Intent intent = new Intent(action);
        intent.setClass(mAppContext, NotificationService.Receiver.class);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_ID, notificationId);

        return PendingIntent.getBroadcast(mAppContext, platformId, intent,
                PendingIntent.FLAG_UPDATE_CURRENT);
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
        if (icon == null || icon.getWidth() == 0) {
            icon = getIconGenerator().generateIconForUrl(origin);
        }

        Notification.Builder notificationBuilder = new Notification.Builder(mAppContext)
                .setContentTitle(title)
                .setContentText(body)
                .setStyle(new Notification.BigTextStyle().bigText(body))
                .setLargeIcon(icon)
                .setSmallIcon(R.drawable.notification_badge)
                .setContentIntent(getPendingIntent(
                        notificationId, mLastNotificationId,
                        NotificationConstants.ACTION_CLICK_NOTIFICATION))
                .setDeleteIntent(getPendingIntent(
                        notificationId, mLastNotificationId,
                        NotificationConstants.ACTION_CLOSE_NOTIFICATION))
                .setSubText(origin);

        if (sObserver != null) {
            sObserver.onBeforeDisplayNotification(notificationBuilder, origin);
        }

        mNotificationManager.notify(mLastNotificationId, notificationBuilder.build());

        return mLastNotificationId++;
    }

    /**
     * Ensures the existance of an icon generator, which is created lazily.
     *
     * @return The icon generator which can be used.
     */
    private RoundedIconGenerator getIconGenerator() {
        if (mIconGenerator == null) {
            Resources res = mAppContext.getResources();
            float density = res.getDisplayMetrics().density;

            int widthPx = res.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
            int heightPx =
                    res.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);

            mIconGenerator = new RoundedIconGenerator(
                    mAppContext,
                    (int) (widthPx / density),
                    (int) (heightPx / density),
                    (int) (Math.min(widthPx, heightPx) / density / 2),
                    NOTIFICATION_ICON_BG_COLOR,
                    NOTIFICATION_TEXT_SIZE_DP);
        }

        return mIconGenerator;
    }

    /**
     * Closes the notification identified by |platformId|.
     */
    @CalledByNative
    private void closeNotification(int platformId) {
        mNotificationManager.cancel(platformId);
    }

    private boolean onNotificationClicked(String notificationId) {
        return nativeOnNotificationClicked(mNativeNotificationManager, notificationId);
    }

    private boolean onNotificationClosed(String notificationId) {
        return nativeOnNotificationClosed(mNativeNotificationManager, notificationId);
    }

    private static native void nativeInitializeNotificationUIManager();

    private native boolean nativeOnNotificationClicked(
            long nativeNotificationUIManagerAndroid, String notificationId);
    private native boolean nativeOnNotificationClosed(
            long nativeNotificationUIManagerAndroid, String notificationId);
}
