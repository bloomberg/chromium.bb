// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.Context;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;

/**
 * Factory which supplies the appropriate type of notification builder based on Android version.
 * Should be used for all notifications we create, to ensure a notification channel is set on O.
 */
public class NotificationBuilderFactory {
    /**
     * Creates either a Notification.Builder or NotificationCompat.Builder under the hood, wrapped
     * in our own common interface.
     *
     * TODO(crbug.com/704152) Remove this once we've updated to revision 26 of the support library.
     * Then we can use NotificationCompat.Builder and set the channel directly everywhere.
     *
     * @param preferCompat true if a NotificationCompat.Builder is preferred.
     *                     A Notification.Builder will be used regardless on Android O.
     * @param channelId The ID of the channel the notification should be posted to.
     * @param channelName The name of the channel the notification should be posted to.
     *                    Used to create the channel if a channel with channelId does not exist.
     * @param channelGroupId The ID of the channel group the channel belongs to. Used when creating
     *                       the channel if a channel with channelId does not exist.
     * @param channelGroupName The name of the channel group the channel belongs to.
     *                         Used to create the channel group if a channel group with
     *                         channelGroupId does not already exist.
     */
    public static ChromeNotificationBuilder createChromeNotificationBuilder(boolean preferCompat,
            String channelId, String channelName, String channelGroupId, String channelGroupName) {
        Context context = ContextUtils.getApplicationContext();
        if (BuildInfo.isAtLeastO()) {
            return new NotificationBuilderForO(ContextUtils.getApplicationContext(), channelId,
                    channelName, channelGroupId, channelGroupName);
        }
        return preferCompat ? new NotificationCompatBuilder(context)
                            : new NotificationBuilder(context);
    }
}
