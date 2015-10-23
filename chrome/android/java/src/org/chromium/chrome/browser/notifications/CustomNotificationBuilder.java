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

import java.util.Arrays;
import java.util.Date;

/**
 * Builds a notification using the given inputs. Uses RemoteViews to provide a custom layout.
 */
public class CustomNotificationBuilder implements NotificationBuilder {
    private final Context mContext;
    private String mTitle;
    private String mBody;
    private String mOrigin;
    private CharSequence mTickerText;
    private Bitmap mLargeIcon;
    private int mSmallIconId;
    private PendingIntent mContentIntent;
    private PendingIntent mDeleteIntent;
    private int mDefaults;
    private long[] mVibratePattern;

    public CustomNotificationBuilder(Context context) {
        mContext = context;
    }

    @Override
    public Notification build() {
        RemoteViews compactView =
                new RemoteViews(mContext.getPackageName(), R.layout.web_notification);
        RemoteViews bigView =
                new RemoteViews(mContext.getPackageName(), R.layout.web_notification_big);

        String time = DateFormat.getTimeFormat(mContext).format(new Date());
        for (RemoteViews view : new RemoteViews[] {compactView, bigView}) {
            view.setTextViewText(R.id.time, time);
            view.setTextViewText(R.id.title, mTitle);
            view.setTextViewText(R.id.body, mBody);
            view.setTextViewText(R.id.origin, mOrigin);
            view.setImageViewBitmap(R.id.icon, mLargeIcon);
        }

        NotificationCompat.Builder builder = new NotificationCompat.Builder(mContext);
        builder.setTicker(mTickerText);
        builder.setSmallIcon(mSmallIconId);
        builder.setContentIntent(mContentIntent);
        builder.setDeleteIntent(mDeleteIntent);
        builder.setDefaults(mDefaults);
        builder.setVibrate(mVibratePattern);
        builder.setContent(compactView);

        Notification notification = builder.build();
        notification.bigContentView = bigView;
        return notification;
    }

    @Override
    public NotificationBuilder setTitle(String title) {
        mTitle = title;
        return this;
    }

    @Override
    public NotificationBuilder setBody(String body) {
        mBody = body;
        return this;
    }

    @Override
    public NotificationBuilder setOrigin(String origin) {
        mOrigin = origin;
        return this;
    }

    @Override
    public NotificationBuilder setTicker(CharSequence tickerText) {
        mTickerText = tickerText;
        return this;
    }

    @Override
    public NotificationBuilder setLargeIcon(Bitmap icon) {
        mLargeIcon = icon;
        return this;
    }

    @Override
    public NotificationBuilder setSmallIcon(int iconId) {
        mSmallIconId = iconId;
        return this;
    }

    @Override
    public NotificationBuilder setContentIntent(PendingIntent intent) {
        mContentIntent = intent;
        return this;
    }

    @Override
    public NotificationBuilder setDeleteIntent(PendingIntent intent) {
        mDeleteIntent = intent;
        return this;
    }

    @Override
    public NotificationBuilder addAction(int iconId, CharSequence title, PendingIntent intent) {
        // TODO(mvanouwerkerk): Implement as part of issue 541617.
        return this;
    }

    @Override
    public NotificationBuilder setDefaults(int defaults) {
        mDefaults = defaults;
        return this;
    }

    @Override
    public NotificationBuilder setVibrate(long[] pattern) {
        mVibratePattern = Arrays.copyOf(pattern, pattern.length);
        return this;
    }
}
