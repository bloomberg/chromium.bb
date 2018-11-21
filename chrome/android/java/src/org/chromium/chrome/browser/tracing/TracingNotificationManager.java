// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

/**
 * Manages notifications displayed while tracing and once tracing is complete.
 */
public class TracingNotificationManager {
    private static final String TRACING_NOTIFICATION_TAG = "tracing_status";
    private static final int TRACING_NOTIFICATION_ID = 100;

    private static NotificationManagerProxy sNotificationManagerOverride;

    // TODO(eseckler): Consider recording UMAs, see e.g. IncognitoNotificationManager.

    private static NotificationManagerProxy getNotificationManager(Context context) {
        return sNotificationManagerOverride != null ? sNotificationManagerOverride
                                                    : new NotificationManagerProxyImpl(context);
    }

    /**
     * Instruct the TracingNotificationManager to use a different NotificationManager during a test.
     *
     * @param notificationManager the manager to use instead.
     */
    @VisibleForTesting
    public static void overrideNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        sNotificationManagerOverride = notificationManager;
    }

    /**
     * @return whether notifications posted to the BROWSER notification channel are enabled by
     * the user. True if the state can't be determined.
     */
    public static boolean browserNotificationsEnabled() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            // Can't determine the state, so assume they are enabled.
            return true;
        }

        if (!getNotificationManager(ContextUtils.getApplicationContext())
                        .areNotificationsEnabled()) {
            return false;
        }

        // On Android O and above, the BROWSER channel may have independently been disabled, too.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return notificationChannelEnabled(ChannelDefinitions.ChannelId.BROWSER);
        }

        return true;
    }

    @TargetApi(Build.VERSION_CODES.O)
    private static boolean notificationChannelEnabled(String channelId) {
        NotificationChannel channel = getNotificationManager(ContextUtils.getApplicationContext())
                                              .getNotificationChannel(channelId);
        // Can't determine the state if the channel doesn't exist, assume notifications are enabled.
        if (channel == null) return true;
        return channel.getImportance() != NotificationManager.IMPORTANCE_NONE;
    }

    /**
     * Replace the tracing notification with one indicating that a trace is being recorded.
     */
    public static void showTracingActiveNotification() {
        Context context = ContextUtils.getApplicationContext();
        String title = context.getResources().getString(R.string.tracing_active_notification_title);
        // TODO(eseckler): Update the buffer usage in the notification periodically.
        int bufferUsagePercentage = 0;
        String message = context.getResources().getString(
                R.string.tracing_active_notification_message, bufferUsagePercentage);

        ChromeNotificationBuilder builder =
                createNotificationBuilder()
                        .setContentTitle(title)
                        .setContentText(message)
                        .setOngoing(true)
                        .addAction(R.drawable.ic_stop_white_36dp,
                                ContextUtils.getApplicationContext().getResources().getString(
                                        R.string.tracing_stop),
                                TracingNotificationService.getStopRecordingIntent(context));
        showNotification(builder.build());
    }

    /**
     * Replace the tracing notification with one indicating that a trace is being finalized.
     */
    public static void showTracingStoppingNotification() {
        Context context = ContextUtils.getApplicationContext();
        String title =
                context.getResources().getString(R.string.tracing_stopping_notification_title);
        String message =
                context.getResources().getString(R.string.tracing_stopping_notification_message);

        ChromeNotificationBuilder builder = createNotificationBuilder()
                                                    .setContentTitle(title)
                                                    .setContentText(message)
                                                    .setOngoing(true);
        showNotification(builder.build());
    }

    /**
     * Replace the tracing notification with one indicating that a trace was recorded successfully.
     */
    public static void showTracingCompleteNotification() {
        Context context = ContextUtils.getApplicationContext();
        String title =
                context.getResources().getString(R.string.tracing_complete_notification_title);
        String message =
                context.getResources().getString(R.string.tracing_complete_notification_message);

        ChromeNotificationBuilder builder =
                createNotificationBuilder()
                        .setContentTitle(title)
                        .setContentText(message)
                        .setOngoing(false)
                        .addAction(R.drawable.ic_share_white_24dp,
                                ContextUtils.getApplicationContext().getResources().getString(
                                        R.string.tracing_share),
                                TracingNotificationService.getShareTraceIntent(context))
                        .setDeleteIntent(TracingNotificationService.getDiscardTraceIntent(context));
        showNotification(builder.build());
    }

    /**
     * Dismiss any active tracing notification if there is one.
     */
    public static void dismissNotification() {
        NotificationManagerProxy manager =
                getNotificationManager(ContextUtils.getApplicationContext());
        manager.cancel(TRACING_NOTIFICATION_TAG, TRACING_NOTIFICATION_ID);
    }

    private static ChromeNotificationBuilder createNotificationBuilder() {
        return NotificationBuilderFactory
                .createChromeNotificationBuilder(
                        true /* preferCompat */, ChannelDefinitions.ChannelId.BROWSER)
                .setVisibility(Notification.VISIBILITY_PUBLIC)
                .setSmallIcon(R.drawable.ic_chrome)
                .setShowWhen(false)
                .setLocalOnly(true);
    }

    private static void showNotification(Notification notification) {
        NotificationManagerProxy manager =
                getNotificationManager(ContextUtils.getApplicationContext());
        manager.notify(TRACING_NOTIFICATION_TAG, TRACING_NOTIFICATION_ID, notification);
    }
}
