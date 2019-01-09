// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Implements notifications when pages are automatically fetched after reaching the net-error page.
 */
@JNINamespace("offline_pages")
public class AutoFetchNotifier {
    private static final String NOTIFICATION_TAG = "OfflinePageAutoFetchNotification";
    private static final String EXTRA_URL = "org.chromium.chrome.browser.offlinepages.URL";

    /**
     * Opens the offline page when the notification is tapped.
     */
    public static class ClickReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(final Context context, Intent intent) {
            // TODO(crbug.com/883486): Record UMA.

            // Create a new intent that will be handled by |ChromeTabbedActivity| to open the page.
            // This |BroadcastReceiver| is only required for collecting UMA.
            Intent viewIntent = new Intent(Intent.ACTION_VIEW,
                    Uri.parse(IntentUtils.safeGetStringExtra(intent, EXTRA_URL)));
            viewIntent.putExtras(intent);
            viewIntent.setPackage(context.getPackageName());
            viewIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(viewIntent);
        }
    }

    /**
     * Creates a system notification that informs the user when an auto-fetched page is ready.
     * If the notification is tapped, it opens the offline page in Chrome.
     *
     * @param pageTitle     The title of the page. This is displayed on the notification.
     * @param originalUrl   The originally requested URL (before any redirection).
     * @param tabId         ID of the tab where the auto-fetch occurred. This tab is used, if
     *                      available, to open the offline page when the notification is tapped.
     * @param offlineId     The offlineID for the offline page that was just saved.
     */
    @CalledByNative
    public static void showCompleteNotification(
            String pageTitle, String originalUrl, int tabId, long offlineId) {
        Context context = ContextUtils.getApplicationContext();
        OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                originalUrl, offlineId, LaunchLocation.NOTIFICATION, (params) -> {
                    showCompleteNotificationWithParams(
                            pageTitle, tabId, offlineId, originalUrl, params);
                });
    }

    private static void showCompleteNotificationWithParams(
            String pageTitle, int tabId, long offlineId, String originalUrl, LoadUrlParams params) {
        Context context = ContextUtils.getApplicationContext();
        // Create an intent to be handled by ClickReceiver.
        Intent intent = new Intent(context, ClickReceiver.class);
        intent.putExtra(EXTRA_URL, originalUrl);
        IntentHandler.setIntentExtraHeaders(params.getExtraHeaders(), intent);
        intent.putExtra(TabOpenType.REUSE_TAB_MATCHING_ID_STRING, tabId);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_CHROME_UI);

        intent.setPackage(context.getPackageName());

        PendingIntent clickIntent = PendingIntent.getBroadcast(
                context, (int) offlineId /* requestCode */, intent, 0 /* flags */);

        // Create the notification.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(
                                true /* preferCompat */, ChannelDefinitions.ChannelId.DOWNLOADS)
                        .setAutoCancel(true)
                        .setContentIntent(clickIntent)
                        .setContentTitle(pageTitle)
                        .setContentText(context.getString(
                                R.string.offline_pages_auto_fetch_ready_notification_text))
                        .setGroup(NOTIFICATION_TAG)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_LOW)
                        .setSmallIcon(R.drawable.ic_chrome);

        Notification notification = builder.build();
        // Use the offline ID for a unique notification ID. Offline ID is a random
        // 64-bit integer. Truncating to 32 bits isn't ideal, but chances of collision
        // is still very low, and users should have few of these notifications
        // anyway.
        int notificationId = (int) offlineId;
        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.notify(NOTIFICATION_TAG, notificationId, notification);
    }
}
