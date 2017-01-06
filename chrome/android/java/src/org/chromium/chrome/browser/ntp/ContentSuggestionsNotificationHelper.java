// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.annotation.TargetApi;
import android.app.AlarmManager;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.service.notification.StatusBarNotification;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

/**
 * Provides functionality needed for content suggestion notifications.
 *
 * Exposes helper functions to native C++ code.
 */
public class ContentSuggestionsNotificationHelper {
    private static final String NOTIFICATION_TAG = "ContentSuggestionsNotification";
    private static final String NOTIFICATION_ID_EXTRA = "notification_id";

    private ContentSuggestionsNotificationHelper() {} // Prevent instantiation

    /**
     * Removes the notification after a timeout period.
     */
    public static final class TimeoutReceiver extends BroadcastReceiver {
        public void onReceive(Context context, Intent intent) {
            int id = intent.getIntExtra(NOTIFICATION_ID_EXTRA, -1);
            if (id < 0) return;
            hideNotification(id);
        }
    }

    @CalledByNative
    private static void openUrl(String url) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        intent.setClass(context, ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.addTrustedIntentExtras(intent);
        context.startActivity(intent);
    }

    @TargetApi(Build.VERSION_CODES.M)
    @CalledByNative
    private static void showNotification(
            String url, String title, String text, Bitmap image, long timeoutAtMillis) {
        // Post notification.
        Context context = ContextUtils.getApplicationContext();
        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);

        // Find an available notification ID.
        int nextId = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            for (StatusBarNotification activeNotification : manager.getActiveNotifications()) {
                if (activeNotification.getTag() != NOTIFICATION_TAG) continue;
                if (activeNotification.getId() >= nextId) {
                    nextId = activeNotification.getId() + 1;
                }
            }
        }

        Intent intent = new Intent()
                                .setAction(Intent.ACTION_VIEW)
                                .setData(Uri.parse(url))
                                .setClass(context, ChromeLauncherActivity.class)
                                .putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true)
                                .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(context)
                        .setAutoCancel(true)
                        .setContentIntent(PendingIntent.getActivity(context, 0, intent, 0))
                        .setContentTitle(title)
                        .setContentText(text)
                        .setGroup(NOTIFICATION_TAG)
                        .setLargeIcon(image)
                        .setSmallIcon(R.drawable.ic_chrome);
        manager.notify(NOTIFICATION_TAG, nextId, builder.build());

        // Set timeout.
        if (timeoutAtMillis != Long.MAX_VALUE) {
            AlarmManager alarmManager =
                    (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
            Intent timeoutIntent = new Intent(context, TimeoutReceiver.class)
                                           .setData(Uri.parse(url))
                                           .putExtra(NOTIFICATION_ID_EXTRA, nextId);
            alarmManager.set(AlarmManager.RTC, timeoutAtMillis,
                    PendingIntent.getBroadcast(
                            context, 0, timeoutIntent, PendingIntent.FLAG_UPDATE_CURRENT));
        }
    }

    private static void hideNotification(int id) {
        Context context = ContextUtils.getApplicationContext();
        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(NOTIFICATION_TAG, id);
    }

    @TargetApi(Build.VERSION_CODES.M)
    @CalledByNative
    private static void hideAllNotifications() {
        Context context = ContextUtils.getApplicationContext();
        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            for (StatusBarNotification activeNotification : manager.getActiveNotifications()) {
                if (activeNotification.getTag() == NOTIFICATION_TAG) {
                    manager.cancel(NOTIFICATION_TAG, activeNotification.getId());
                }
            }
        } else {
            manager.cancel(NOTIFICATION_TAG, 0);
        }
    }
}
