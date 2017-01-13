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
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.service.notification.StatusBarNotification;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

/**
 * Provides functionality needed for content suggestion notifications.
 *
 * Exposes helper functions to native C++ code.
 */
@JNINamespace("ntp_snippets")
public class ContentSuggestionsNotificationHelper {
    private static final String NOTIFICATION_TAG = "ContentSuggestionsNotification";
    private static final String NOTIFICATION_ID_EXTRA = "notification_id";

    private static final String PREF_CACHED_ACTION_TAP =
            "ntp.content_suggestions.notification.cached_action_tap";
    private static final String PREF_CACHED_ACTION_DISMISSAL =
            "ntp.content_suggestions.notification.cached_action_dismissal";
    private static final String PREF_CACHED_ACTION_HIDE_DEADLINE =
            "ntp.content_suggestions.notification.cached_action_hide_deadline";
    private static final String PREF_CACHED_CONSECUTIVE_IGNORED =
            "ntp.content_suggestions.notification.cached_consecutive_ignored";

    private ContentSuggestionsNotificationHelper() {} // Prevent instantiation

    /**
     * Opens the content suggestion when notification is tapped.
     */
    public static final class OpenUrlReceiver extends BroadcastReceiver {
        public void onReceive(Context context, Intent intent) {
            openUrl(intent.getData());
            recordCachedActionMetric(PREF_CACHED_ACTION_TAP);
        }
    }

    /**
     * Records dismissal when notification is swiped away.
     */
    public static final class DeleteReceiver extends BroadcastReceiver {
        public void onReceive(Context context, Intent intent) {
            recordCachedActionMetric(PREF_CACHED_ACTION_DISMISSAL);
        }
    }

    /**
     * Removes the notification after a timeout period.
     */
    public static final class TimeoutReceiver extends BroadcastReceiver {
        public void onReceive(Context context, Intent intent) {
            int id = intent.getIntExtra(NOTIFICATION_ID_EXTRA, -1);
            if (id < 0) return;
            hideNotification(id);
            recordCachedActionMetric(PREF_CACHED_ACTION_HIDE_DEADLINE);
        }
    }

    private static void openUrl(Uri uri) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent()
                                .setAction(Intent.ACTION_VIEW)
                                .setData(uri)
                                .setClass(context, ChromeLauncherActivity.class)
                                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                                .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName())
                                .putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
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

        Intent contentIntent = new Intent(context, OpenUrlReceiver.class).setData(Uri.parse(url));
        Intent deleteIntent = new Intent(context, DeleteReceiver.class).setData(Uri.parse(url));
        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(context)
                        .setAutoCancel(true)
                        .setContentIntent(PendingIntent.getBroadcast(context, 0, contentIntent, 0))
                        .setDeleteIntent(PendingIntent.getBroadcast(context, 0, deleteIntent, 0))
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

    /**
     * Records that an action was performed on a notification.
     *
     * Also tracks the number of consecutively-ignored notifications, resetting it on a tap or
     * otherwise incrementing it.
     *
     * This method may be called when the native library is not loaded. If it is loaded, the metrics
     * will immediately be sent to C++. If not, it will cache them for a later call to
     * flushCachedMetrics().
     *
     * @param prefName The name of the action metric pref to update (<tt>PREF_CACHED_ACTION_*</tt>)
     */
    private static void recordCachedActionMetric(String prefName) {
        assert prefName.startsWith("ntp.content_suggestions.notification.cached_action_");

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        int currentValue = prefs.getInt(prefName, 0);
        int consecutiveIgnored = 0;
        if (!prefName.equals(PREF_CACHED_ACTION_TAP)) {
            consecutiveIgnored = 1 + prefs.getInt(PREF_CACHED_CONSECUTIVE_IGNORED, 0);
        }
        prefs.edit()
                .putInt(prefName, currentValue + 1)
                .putInt(PREF_CACHED_CONSECUTIVE_IGNORED, consecutiveIgnored)
                .apply();

        if (LibraryLoader.isInitialized()) {
            flushCachedMetrics();
        }
    }

    /**
     * Invokes nativeReceiveFlushedMetrics() with cached metrics and resets them.
     *
     * It may be called from either native or Java code, as long as the native libray is loaded.
     */
    @CalledByNative
    private static void flushCachedMetrics() {
        assert LibraryLoader.isInitialized();

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        int tapCount = prefs.getInt(PREF_CACHED_ACTION_TAP, 0);
        int dismissalCount = prefs.getInt(PREF_CACHED_ACTION_DISMISSAL, 0);
        int hideDeadlineCount = prefs.getInt(PREF_CACHED_ACTION_HIDE_DEADLINE, 0);
        int consecutiveIgnored = prefs.getInt(PREF_CACHED_CONSECUTIVE_IGNORED, 0);

        if (tapCount > 0 || dismissalCount > 0 || hideDeadlineCount > 0) {
            nativeReceiveFlushedMetrics(tapCount, dismissalCount, hideDeadlineCount,
                                        consecutiveIgnored);
            prefs.edit()
                    .remove(PREF_CACHED_ACTION_TAP)
                    .remove(PREF_CACHED_ACTION_DISMISSAL)
                    .remove(PREF_CACHED_ACTION_HIDE_DEADLINE)
                    .remove(PREF_CACHED_CONSECUTIVE_IGNORED)
                    .apply();
        }
    }

    private static native void nativeReceiveFlushedMetrics(
            int tapCount, int dismissalCount, int hideDeadlineCount, int consecutiveIgnored);
}
