// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Build;

/**
 * Wraps a Notification.Builder object.
 */
public class NotificationBuilder implements ChromeNotificationBuilder {
    protected final Notification.Builder mBuilder;

    public NotificationBuilder(Context context) {
        mBuilder = new Notification.Builder(context);
    }

    @Override
    public ChromeNotificationBuilder setAutoCancel(boolean autoCancel) {
        mBuilder.setAutoCancel(autoCancel);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentIntent(PendingIntent contentIntent) {
        mBuilder.setContentIntent(contentIntent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentTitle(String title) {
        mBuilder.setContentTitle(title);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentText(String text) {
        mBuilder.setContentText(text);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setSmallIcon(int icon) {
        mBuilder.setSmallIcon(icon);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setTicker(String text) {
        mBuilder.setTicker(text);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setLocalOnly(boolean localOnly) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            mBuilder.setLocalOnly(localOnly);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setGroup(String group) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            mBuilder.setGroup(group);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setOngoing(boolean ongoing) {
        mBuilder.setOngoing(ongoing);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setVisibility(int visibility) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mBuilder.setVisibility(visibility);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setShowWhen(boolean showWhen) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            mBuilder.setShowWhen(showWhen);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder addAction(int icon, String title, PendingIntent intent) {
        mBuilder.addAction(icon, title, intent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setDeleteIntent(PendingIntent intent) {
        mBuilder.setDeleteIntent(intent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setPriority(int pri) {
        mBuilder.setPriority(pri);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setProgress(int max, int percentage, boolean indeterminate) {
        mBuilder.setProgress(max, percentage, indeterminate);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setSubText(String text) {
        mBuilder.setSubText(text);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentInfo(String info) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            mBuilder.setContentInfo(info);
        } else {
            mBuilder.setSubText(info);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setWhen(long time) {
        mBuilder.setWhen(time);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setLargeIcon(Bitmap icon) {
        mBuilder.setLargeIcon(icon);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setVibrate(long[] vibratePattern) {
        mBuilder.setVibrate(vibratePattern);
        return this;
    }

    @Override
    public Notification buildWithBigTextStyle(String bigText) {
        Notification.BigTextStyle bigTextStyle = new Notification.BigTextStyle(mBuilder);
        bigTextStyle.bigText(bigText);
        return bigTextStyle.build();
    }

    @Override
    public Notification build() {
        return mBuilder.build();
    }
}