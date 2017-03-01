// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.graphics.Bitmap;

/**
 * Abstraction over Notification.Builder and NotificationCompat.Builder interfaces.
 *
 * TODO(awdf) Remove this once we've updated to revision 26 of the support library.
 */
public interface ChromeNotificationBuilder {
    ChromeNotificationBuilder setAutoCancel(boolean autoCancel);

    ChromeNotificationBuilder setContentIntent(PendingIntent contentIntent);

    ChromeNotificationBuilder setContentTitle(String title);

    ChromeNotificationBuilder setContentText(String text);

    ChromeNotificationBuilder setSmallIcon(int icon);

    ChromeNotificationBuilder setTicker(String text);

    ChromeNotificationBuilder setLocalOnly(boolean localOnly);

    ChromeNotificationBuilder setGroup(String group);

    ChromeNotificationBuilder setOngoing(boolean ongoing);

    ChromeNotificationBuilder setVisibility(int visibility);

    ChromeNotificationBuilder setShowWhen(boolean showWhen);

    ChromeNotificationBuilder addAction(int icon, String title, PendingIntent intent);

    ChromeNotificationBuilder setDeleteIntent(PendingIntent intent);

    ChromeNotificationBuilder setPriority(int pri);

    ChromeNotificationBuilder setProgress(int max, int percentage, boolean indeterminate);

    ChromeNotificationBuilder setSubText(String text);

    ChromeNotificationBuilder setContentInfo(String info);

    ChromeNotificationBuilder setWhen(long time);

    ChromeNotificationBuilder setLargeIcon(Bitmap icon);

    ChromeNotificationBuilder setVibrate(long[] vibratePattern);

    Notification buildWithBigTextStyle(String bigText);

    Notification build();
}
