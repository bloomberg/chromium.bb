// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.content.Context;

/**
 * Builds a notification using the standard Notification.BigTextStyle layout.
 */
public class StandardNotificationBuilder extends NotificationBuilderBase {
    private final Context mContext;

    public StandardNotificationBuilder(Context context) {
        mContext = context;
    }

    @Override
    public Notification build() {
        // Note: this is not a NotificationCompat builder so be mindful of the
        // API level of methods you call on the builder.
        Notification.Builder builder = new Notification.Builder(mContext);
        builder.setContentTitle(mTitle);
        builder.setContentText(mBody).setStyle(new Notification.BigTextStyle().bigText(mBody));
        builder.setSubText(mOrigin);
        builder.setTicker(mTickerText);
        builder.setLargeIcon(mLargeIcon);
        builder.setSmallIcon(mSmallIconId);
        builder.setContentIntent(mContentIntent);
        builder.setDeleteIntent(mDeleteIntent);
        for (Action action : mActions) {
            addActionToBuilder(builder, action);
        }
        if (mSettingsAction != null) {
            addActionToBuilder(builder, mSettingsAction);
        }
        builder.setDefaults(mDefaults);
        builder.setVibrate(mVibratePattern);
        builder.setWhen(mTimestamp);
        builder.setOnlyAlertOnce(!mRenotify);
        return builder.build();
    }
}
