// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.components.url_formatter.UrlFormatter;

/**
 * Manages the notification shown by Chrome when running standalone Web Apps. It accomplishes
 * number of goals:
 * - Presents the current URL.
 * - Exposes 'Share' and 'Open in Chrome' actions.
 * - Messages that Web App runs in Chrome.
 */
class WebappActionsNotificationManager {
    private static final String ACTION_SHARE =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_SHARE";
    private static final String ACTION_OPEN_IN_CHROME =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_OPEN_IN_CHROME";
    private static final String ACTION_FOCUS =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_FOCUS";

    static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_PERSISTENT_NOTIFICATION);
    }

    private final WebappActivity mWebappActivity;

    public WebappActionsNotificationManager(WebappActivity webappActivity) {
        this.mWebappActivity = webappActivity;
    }

    public void maybeShowNotification() {
        if (!isEnabled() || mWebappActivity.getActivityTab() == null) return;

        // All features provided by the notification are also available in the minimal-ui toolbar.
        if (mWebappActivity.mWebappInfo.displayMode() == WebDisplayMode.MINIMAL_UI) {
            return;
        }

        NotificationManager nm = (NotificationManager) mWebappActivity.getSystemService(
                Context.NOTIFICATION_SERVICE);
        nm.notify(NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS, createNotification());
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.WEBAPP_ACTIONS, ChannelDefinitions.CHANNEL_ID_BROWSER);
    }

    private Notification createNotification() {
        PendingIntent focusIntent = PendingIntent.getActivity(mWebappActivity, 0,
                new Intent(mWebappActivity, mWebappActivity.getClass()).setAction(ACTION_FOCUS),
                PendingIntent.FLAG_UPDATE_CURRENT);

        PendingIntent openInChromeIntent = PendingIntent.getActivity(mWebappActivity, 0,
                new Intent(mWebappActivity, mWebappActivity.getClass())
                        .setAction(ACTION_OPEN_IN_CHROME),
                PendingIntent.FLAG_UPDATE_CURRENT);

        PendingIntent shareIntent = PendingIntent.getActivity(mWebappActivity, 0,
                new Intent(mWebappActivity, mWebappActivity.getClass()).setAction(ACTION_SHARE),
                PendingIntent.FLAG_UPDATE_CURRENT);

        return NotificationBuilderFactory
                .createChromeNotificationBuilder(
                        true /* prefer compat */, ChannelDefinitions.CHANNEL_ID_BROWSER)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(
                        mWebappActivity.getString(R.string.webapp_runs_in_chrome_disclosure,
                                mWebappActivity.mWebappInfo.shortName()))
                .setContentText(
                        UrlFormatter.formatUrlForDisplay(mWebappActivity.getActivityTab().getUrl()))
                .setShowWhen(false)
                .setAutoCancel(false)
                .setOngoing(true)
                .setPriority(Notification.PRIORITY_MIN)
                .setContentIntent(focusIntent)
                .addAction(R.drawable.ic_share_white_24dp,
                        mWebappActivity.getResources().getString(R.string.share), shareIntent)
                .addAction(R.drawable.ic_exit_to_app_white_24dp,
                        mWebappActivity.getResources().getString(R.string.menu_open_in_chrome),
                        openInChromeIntent)
                .build();
    }

    public void cancelNotification() {
        if (!isEnabled()) return;
        NotificationManager nm = (NotificationManager) mWebappActivity.getSystemService(
                Context.NOTIFICATION_SERVICE);
        nm.cancel(NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS);
    }

    public boolean handleNotificationAction(Intent intent) {
        if (ACTION_SHARE.equals(intent.getAction())) {
            // Not routing through onMenuOrKeyboardAction to control UMA String.
            mWebappActivity.onShareMenuItemSelected(
                    false /* share directly */, mWebappActivity.getCurrentTabModel().isIncognito());
            RecordUserAction.record("Webapp.NotificationShare");
            return true;
        } else if (ACTION_OPEN_IN_CHROME.equals(intent.getAction())) {
            mWebappActivity.onMenuOrKeyboardAction(R.id.open_in_browser_id, false /* fromMenu */);
            return true;
        } else if (ACTION_FOCUS.equals(intent.getAction())) {
            // Do nothing, just close notification drawer and focus the Web App.
            RecordUserAction.record("Webapp.NotificationFocused");
            return true;
        }
        return false;
    }
}
