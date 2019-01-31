// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Notification;
import android.content.Context;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/** Class providing utils for when the VR module is not installed. */
/* package */ class VrFallbackUtils {
    private static final int PREPARING_VR_NOTIFICATION_TIMEOUT_MS = 5000;
    private static final int PREPARING_VR_NOTIFICATION_DELAY_MS = 2000;

    /** Shows immersive notification informing the user that the VR browser is not ready yet. */
    public static void showFailureNotification(Context context) {
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
            NotificationManagerProxy notificationManager =
                    new NotificationManagerProxyImpl(context);
            Notification notification =
                    NotificationBuilderFactory
                            .createChromeNotificationBuilder(true, ChannelDefinitions.ChannelId.VR)
                            .setContentTitle(context.getResources().getString(
                                    R.string.vr_preparing_vr_notification_title))
                            .setContentText(context.getResources().getString(
                                    R.string.vr_preparing_vr_notification_body))
                            .setSmallIcon(R.drawable.ic_chrome)
                            // Need to set category and max priority. Otherwise, notification
                            // won't show up.
                            .setCategory(Notification.CATEGORY_MESSAGE)
                            .setPriorityBeforeO(NotificationCompat.PRIORITY_MAX)
                            .build();
            notificationManager.notify(
                    NotificationConstants.NOTIFICATION_ID_PREPARING_VR, notification);

            // Close notification after a few seconds as it is only really relevant right after
            // accessing the VR browser failed.
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
                notificationManager.cancel(NotificationConstants.NOTIFICATION_ID_PREPARING_VR);
            }, PREPARING_VR_NOTIFICATION_TIMEOUT_MS);
        }, PREPARING_VR_NOTIFICATION_DELAY_MS);
    }
}
