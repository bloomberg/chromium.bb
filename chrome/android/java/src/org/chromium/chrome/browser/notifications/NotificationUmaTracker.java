// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.SharedPreferences;
import android.os.Build;
import android.support.annotation.IntDef;
import android.support.v4.app.NotificationManagerCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to make tracking notification UMA stats easier for various features.  Having a
 * single entry point here to make more complex tracking easier to add in the future.
 */
public class NotificationUmaTracker {
    private static final String TAG = "NotifsUMATracker";

    /*
     * A list of notification types.  To add a type to this list please update
     * SystemNotificationType in enums.xml and make sure to keep this list in sync.  Additions
     * should be treated as APPEND ONLY to keep the UMA metric semantics the same over time.
     *
     * A SystemNotificationType value can also be saved in shared preferences.
     */
    @IntDef({SystemNotificationType.UNKNOWN, SystemNotificationType.DOWNLOAD_FILES,
            SystemNotificationType.DOWNLOAD_PAGES, SystemNotificationType.CLOSE_INCOGNITO,
            SystemNotificationType.CONTENT_SUGGESTION, SystemNotificationType.MEDIA_CAPTURE,
            SystemNotificationType.PHYSICAL_WEB, SystemNotificationType.MEDIA,
            SystemNotificationType.SITES, SystemNotificationType.SYNC,
            SystemNotificationType.WEBAPK, SystemNotificationType.BROWSER_ACTIONS,
            SystemNotificationType.WEBAPP_ACTIONS,
            SystemNotificationType.OFFLINE_CONTENT_SUGGESTION,
            SystemNotificationType.TRUSTED_WEB_ACTIVITY_SITES,
            SystemNotificationType.OFFLINE_PAGES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SystemNotificationType {
        int UNKNOWN = -1;
        int DOWNLOAD_FILES = 0;
        int DOWNLOAD_PAGES = 1;
        int CLOSE_INCOGNITO = 2;
        int CONTENT_SUGGESTION = 3;
        int MEDIA_CAPTURE = 4;
        int PHYSICAL_WEB = 5;
        int MEDIA = 6;
        int SITES = 7;
        int SYNC = 8;
        int WEBAPK = 9;
        int BROWSER_ACTIONS = 10;
        int WEBAPP_ACTIONS = 11;
        int OFFLINE_CONTENT_SUGGESTION = 12;
        int TRUSTED_WEB_ACTIVITY_SITES = 13;
        int OFFLINE_PAGES = 14;

        int NUM_ENTRIES = 15;
    }

    /*
     * A list of notification action types, each maps to a notification button.
     * To add a type to this list please update SystemNotificationActionType in enums.xml and make
     * sure to keep this list in sync.  Additions should be treated as APPEND ONLY to keep the UMA
     * metric semantics the same over time.
     */
    @IntDef({ActionType.UNKNOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionType {
        int UNKNOWN = -1;
        // Pause button when an user download is in progress.
        int DOWNLOAD_FILES_IN_PROGRESS_PAUSE = 0;
        // Resume button when an user download is in progress.
        int DOWNLOAD_FILES_IN_PROGRESS_RESUME = 1;
        int NUM_ENTRIES = 2;
    }

    private static final String LAST_SHOWN_NOTIFICATION_TYPE_KEY =
            "NotificationUmaTracker.LastShownNotificationType";

    private static class LazyHolder {
        private static final NotificationUmaTracker INSTANCE = new NotificationUmaTracker();
    }

    /** Cached objects. */
    private final SharedPreferences mSharedPreferences;
    private final NotificationManagerCompat mNotificationManager;

    public static NotificationUmaTracker getInstance() {
        return LazyHolder.INSTANCE;
    }

    private NotificationUmaTracker() {
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
        mNotificationManager = NotificationManagerCompat.from(ContextUtils.getApplicationContext());
    }

    /**
     * Logs {@link android.app.Notification} usage, categorized into {@link SystemNotificationType}
     * types.  Splits the logs by the global enabled state of notifications and also logs the last
     * notification shown prior to the global notifications state being disabled by the user.
     * @param type The type of notification that was shown.
     * @param notification The notification that was shown.
     * @see SystemNotificationType
     */
    public void onNotificationShown(@SystemNotificationType int type, Notification notification) {
        if (type == SystemNotificationType.UNKNOWN) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            logNotificationShown(type, notification.getChannelId());
        } else {
            logNotificationShown(type, null);
        }
    }

    /**
     * Logs notification click event when the user taps on the notification body.
     * @param type Type of the notification.
     */
    public void onNotificationContentClick(@SystemNotificationType int type) {
        if (type == SystemNotificationType.UNKNOWN) return;

        new CachedMetrics
                .EnumeratedHistogramSample("Mobile.SystemNotification.Content.Click",
                        SystemNotificationType.NUM_ENTRIES)
                .record(type);
    }

    /**
     * Logs notification dismiss event the user swipes away the notification.
     * @param type Type of the notification.
     */
    public void onNotificationDismiss(@SystemNotificationType int type) {
        if (type == SystemNotificationType.UNKNOWN) return;

        // TODO(xingliu): This may not work if Android kill Chrome before native library is loaded.
        // Cache data in Android shared preference and flush them to native when available.
        new CachedMetrics
                .EnumeratedHistogramSample(
                        "Mobile.SystemNotification.Dismiss", SystemNotificationType.NUM_ENTRIES)
                .record(type);
    }

    /**
     * Logs notification button click event.
     * @param type Type of the notification action button.
     */
    public void onNotificationActionClick(@ActionType int type) {
        if (type == ActionType.UNKNOWN) return;

        // TODO(xingliu): This may not work if Android kill Chrome before native library is loaded.
        // Cache data in Android shared preference and flush them to native when available.
        new CachedMetrics
                .EnumeratedHistogramSample(
                        "Mobile.SystemNotification.Action.Click", ActionType.NUM_ENTRIES)
                .record(type);
    }

    private void logNotificationShown(
            @SystemNotificationType int type, @ChannelDefinitions.ChannelId String channelId) {
        if (!mNotificationManager.areNotificationsEnabled()) {
            logPotentialBlockedCause();
            recordHistogram("Mobile.SystemNotification.Blocked", type);
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && channelId != null
                && isChannelBlocked(channelId)) {
            recordHistogram("Mobile.SystemNotification.ChannelBlocked", type);
            return;
        }
        saveLastShownNotification(type);
        recordHistogram("Mobile.SystemNotification.Shown", type);
    }

    @TargetApi(26)
    private boolean isChannelBlocked(@ChannelDefinitions.ChannelId String channelId) {
        // Use non-compat notification manager as compat does not have getNotificationChannel (yet).
        NotificationManager notificationManager =
                ContextUtils.getApplicationContext().getSystemService(NotificationManager.class);
        NotificationChannel channel = notificationManager.getNotificationChannel(channelId);
        return channel != null && channel.getImportance() == NotificationManager.IMPORTANCE_NONE;
    }

    private void saveLastShownNotification(@SystemNotificationType int type) {
        mSharedPreferences.edit().putInt(LAST_SHOWN_NOTIFICATION_TYPE_KEY, type).apply();
    }

    private void logPotentialBlockedCause() {
        int lastType = mSharedPreferences.getInt(
                LAST_SHOWN_NOTIFICATION_TYPE_KEY, SystemNotificationType.UNKNOWN);
        if (lastType == -1) return;
        mSharedPreferences.edit().remove(LAST_SHOWN_NOTIFICATION_TYPE_KEY).apply();

        recordHistogram("Mobile.SystemNotification.BlockedAfterShown", lastType);
    }

    private static void recordHistogram(String name, @SystemNotificationType int type) {
        if (type == SystemNotificationType.UNKNOWN) return;

        if (!LibraryLoader.getInstance().isInitialized()) return;
        RecordHistogram.recordEnumeratedHistogram(name, type, SystemNotificationType.NUM_ENTRIES);
    }
}
