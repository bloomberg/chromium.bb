// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.provider.Browser;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashSet;
import java.util.Set;

/**
 * Manages all SendTabToSelf related notifications for Android. This includes displaying, handling
 * taps, and timeouts.
 */
public class NotificationManager {
    private static final String NOTIFICATION_GUID_EXTRA = "send_tab_to_self.notification.guid";

    // Tracks which GUIDs there is an active notification for.
    private static final String PREF_ACTIVE_NOTIFICATIONS = "send_tab_to_self.notification.active";
    private static final String PREF_NEXT_NOTIFICATION_ID = "send_tab_to_self.notification.next_id";

    /** Records dismissal when notification is swiped away. */
    public static final class DeleteReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String guid = intent.getStringExtra(NOTIFICATION_GUID_EXTRA);
            hideNotification(guid);
            SendTabToSelfAndroidBridge.dismissEntry(Profile.getLastUsedProfile(), guid);
        }
    }

    /** Handles the tapping of a notification by opening the URL and hiding the notification. */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            openUrl(intent.getData());
            String guid = intent.getStringExtra(NOTIFICATION_GUID_EXTRA);
            hideNotification(guid);
            SendTabToSelfAndroidBridge.deleteEntry(Profile.getLastUsedProfile(), guid);
        }
    }

    /** Removes the notification after a timeout period. */
    public static final class TimeoutReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String guid = intent.getStringExtra(NOTIFICATION_GUID_EXTRA);
            hideNotification(guid);
            SendTabToSelfAndroidBridge.dismissEntry(Profile.getLastUsedProfile(), guid);
        }
    }

    /**
     * Open the URL specified within Chrome.
     *
     * @param uri The URI to open.
     */
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

    /**
     * Hides a notification and records an action to the Actions histogram.
     *
     * <p>If the notification is not actually visible, then no action will be taken, and the action
     * will not be recorded.
     *
     * @param guid The GUID of the notification to hide.
     */
    @CalledByNative
    private static void hideNotification(@Nullable String guid) {
        ActiveNotification activeNotification = findActiveNotification(guid);
        if (!removeActiveNotification(guid)) {
            return;
        }
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);
        manager.cancel(
                NotificationConstants.GROUP_SEND_TAB_TO_SELF, activeNotification.notificationId);
    }

    /**
     * Displays a notification.
     *
     * @param url URL to open when the user taps on the notification.
     * @param title Title to display within the notification.
     * @param timeoutAtMillis Specifies how long until the notification should be automatically
     *     hidden.
     * @return whether the notification was successfully displayed
     */
    @CalledByNative
    private static boolean showNotification(
            String guid, @NonNull String url, String title, long timeoutAtMillis) {
        // A notification associated with this Share entry already exists. Don't display a new one.
        if (findActiveNotification(guid) != null) {
            return false;
        }

        // Post notification.
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);

        int nextId = getNextNotificationId();
        Uri uri = Uri.parse(url);
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context, nextId,
                new Intent(context, TapReceiver.class)
                        .setData(uri)
                        .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                0);
        PendingIntentProvider deleteIntent = PendingIntentProvider.getBroadcast(context, nextId,
                new Intent(context, DeleteReceiver.class)
                        .setData(uri)
                        .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                0);

        // Build the notification itself.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */,
                                ChannelDefinitions.ChannelId.BROWSER,
                                null /* remoteAppPackageName */,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType
                                                .SEND_TAB_TO_SELF,
                                        NotificationConstants.GROUP_SEND_TAB_TO_SELF, nextId))
                        .setContentIntent(contentIntent)
                        .setDeleteIntent(deleteIntent)
                        .setContentTitle(title)
                        .setContentText(url)
                        .setGroup(NotificationConstants.GROUP_SEND_TAB_TO_SELF)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setDefaults(Notification.DEFAULT_ALL);
        ChromeNotification notification = builder.buildChromeNotification();

        manager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SEND_TAB_TO_SELF,
                notification.getNotification());

        addActiveNotification(new ActiveNotification(nextId, guid));

        // Set timeout.
        if (timeoutAtMillis != Long.MAX_VALUE) {
            AlarmManager alarmManager =
                    (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
            Intent timeoutIntent = new Intent(context, TimeoutReceiver.class)
                                           .setData(Uri.parse(url))
                                           .putExtra(NOTIFICATION_GUID_EXTRA, guid);
            alarmManager.set(AlarmManager.RTC, timeoutAtMillis,
                    PendingIntent.getBroadcast(
                            context, nextId, timeoutIntent, PendingIntent.FLAG_UPDATE_CURRENT));
        }
        return true;
    }

    /**
     * Returns a non-negative integer greater than any active notification's notification ID. Once
     * this id hits close to the INT_MAX_VALUE (unlikely), gets reset to 0.
     */
    private static int getNextNotificationId() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        int nextId = prefs.getInt(PREF_NEXT_NOTIFICATION_ID, -1);
        // Reset the counter when it gets close to max value
        if (nextId == Integer.MAX_VALUE - 1) {
            nextId = -1;
        }
        nextId++;
        prefs.edit().putInt(PREF_NEXT_NOTIFICATION_ID, nextId).apply();
        return nextId;
    }

    /**
     * State of all notifications currently being displayed to the user. It consists of the
     * notification id and GUID of the Share Entry.
     *
     * <p>TODO(https://crbug.com/939026): Consider moving this to another class and add versioning.
     */
    private static class ActiveNotification {
        public final int notificationId;
        public final String guid;

        ActiveNotification(int notificationId, String guid) {
            this.notificationId = notificationId;
            this.guid = guid;
        }

        static ActiveNotification deserialize(String notificationString) {
            String[] tokens = notificationString.split("_");
            if (tokens.length != 2) {
                return null;
            }
            return new ActiveNotification(Integer.parseInt(tokens[0]), tokens[1]);
        }

        /** Serializes the fields to a notificationId_guid string format */
        public String serialize() {
            return new StringBuilder().append(notificationId).append("_").append(guid).toString();
        }
    }

    /**
     * Returns a mutable copy of the named pref. Never returns null.
     *
     * @param prefs The SharedPreferences to retrieve the set of strings from.
     * @param prefName The name of the preference to retrieve.
     * @return Existing set of strings associated with the prefName. If none exists, creates a new
     *     set.
     */
    private static @NonNull Set<String> getMutableStringSetPreference(
            SharedPreferences prefs, String prefName) {
        Set<String> prefValue = prefs.getStringSet(prefName, null);
        if (prefValue == null) {
            return new HashSet<String>();
        }
        return new HashSet<String>(prefValue);
    }

    /**
     * Adds notification to the "active" set.
     *
     * @param notification Notification to be inserted into the active set.
     */
    private static void addActiveNotification(ActiveNotification notification) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> activeNotifications =
                getMutableStringSetPreference(prefs, PREF_ACTIVE_NOTIFICATIONS);
        boolean added = activeNotifications.add(notification.serialize());
        if (added) {
            prefs.edit().putStringSet(PREF_ACTIVE_NOTIFICATIONS, activeNotifications).apply();
        }
    }

    /**
     * Removes notification from the "active" set.
     *
     * @param guid The GUID of the notification to remove.
     * @return whether the notification could be found in the active set and successfully removed.
     */
    private static boolean removeActiveNotification(@Nullable String guid) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        ActiveNotification notification = findActiveNotification(guid);
        if (notification == null || guid == null) {
            return false;
        }

        Set<String> activeNotifications =
                getMutableStringSetPreference(prefs, PREF_ACTIVE_NOTIFICATIONS);
        boolean removed = activeNotifications.remove(notification.serialize());

        if (removed) {
            prefs.edit().putStringSet(PREF_ACTIVE_NOTIFICATIONS, activeNotifications).apply();
        }
        return removed;
    }

    /**
     * Returns an ActiveNotification corresponding to the GUID.
     *
     * @param guid The GUID of the notification to retrieve.
     * @return The Active Notification associated with the passed in GUID. May be null if none
     *         found.
     */
    @Nullable
    private static ActiveNotification findActiveNotification(@Nullable String guid) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> activeNotifications = prefs.getStringSet(PREF_ACTIVE_NOTIFICATIONS, null);
        if (activeNotifications == null || guid == null) {
            return null;
        }

        for (String serialized : activeNotifications) {
            ActiveNotification activeNotification = ActiveNotification.deserialize(serialized);
            if ((activeNotification != null) && (guid.equals(activeNotification.guid))) {
                return activeNotification;
            }
        }
        return null;
    }
}
