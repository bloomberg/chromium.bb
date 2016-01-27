// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.content.Context;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationCompat.Action;

/**
 * Builds a notification using the given inputs. Relies on NotificationCompat and
 * NotificationCompat.BigTextStyle to provide a standard layout.
 */
public class StandardNotificationBuilder extends NotificationBuilderBase {
    private final Context mContext;

    public StandardNotificationBuilder(Context context) {
        mContext = context;
    }

    @Override
    public Notification build() {
        NotificationCompat.Builder builder = new NotificationCompat.Builder(mContext);
        builder.setContentTitle(mTitle);
        builder.setContentText(mBody).setStyle(
                new NotificationCompat.BigTextStyle().bigText(mBody));
        builder.setSubText(mOrigin);
        builder.setTicker(mTickerText);
        builder.setLargeIcon(mLargeIcon);
        builder.setSmallIcon(mSmallIconId);
        builder.setContentIntent(mContentIntent);
        builder.setDeleteIntent(mDeleteIntent);
        for (Action action : mActions) {
            builder.addAction(action);
        }
        if (mSettingsAction != null) {
            builder.addAction(mSettingsAction);
        }
        builder.setDefaults(mDefaults);
        builder.setVibrate(mVibratePattern);
        return builder.build();
    }
}
