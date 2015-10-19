// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.support.v4.app.NotificationCompat;
import android.text.format.DateFormat;
import android.widget.RemoteViews;

import org.chromium.chrome.R;

import java.util.Date;

/**
 * Builds a notification using the given inputs. Uses a RemoteViews instance in order to provide a
 * custom layout.
 */
public class CustomNotificationBuilder implements NotificationBuilder {
    private final NotificationCompat.Builder mBuilder;
    private final RemoteViews mView;

    public CustomNotificationBuilder(Context context) {
        mView = new RemoteViews(context.getPackageName(), R.layout.web_notification);
        mView.setTextViewText(R.id.time, DateFormat.getTimeFormat(context).format(new Date()));
        mBuilder = new NotificationCompat.Builder(context);
    }

    @Override
    public Notification build() {
        return mBuilder.setContent(mView).build();
    }

    @Override
    public NotificationBuilder setTitle(String title) {
        mView.setTextViewText(R.id.title, title);
        return this;
    }

    @Override
    public NotificationBuilder setBody(String body) {
        mView.setTextViewText(R.id.body, body);
        return this;
    }

    @Override
    public NotificationBuilder setOrigin(String origin) {
        mView.setTextViewText(R.id.origin, origin);
        return this;
    }

    @Override
    public NotificationBuilder setTicker(CharSequence tickerText) {
        mBuilder.setTicker(tickerText);
        return this;
    }

    @Override
    public NotificationBuilder setLargeIcon(Bitmap icon) {
        mView.setImageViewBitmap(R.id.icon, icon);
        return this;
    }

    @Override
    public NotificationBuilder setSmallIcon(int iconId) {
        mBuilder.setSmallIcon(iconId);
        return this;
    }

    @Override
    public NotificationBuilder setContentIntent(PendingIntent intent) {
        mBuilder.setContentIntent(intent);
        return this;
    }

    @Override
    public NotificationBuilder setDeleteIntent(PendingIntent intent) {
        mBuilder.setDeleteIntent(intent);
        return this;
    }

    @Override
    public NotificationBuilder addAction(int iconId, CharSequence title, PendingIntent intent) {
        // TODO(mvanouwerkerk): Implement as part of issue 541617.
        return this;
    }

    @Override
    public NotificationBuilder setDefaults(int defaults) {
        mBuilder.setDefaults(defaults);
        return this;
    }

    @Override
    public NotificationBuilder setVibrate(long[] pattern) {
        mBuilder.setVibrate(pattern);
        return this;
    }
}
