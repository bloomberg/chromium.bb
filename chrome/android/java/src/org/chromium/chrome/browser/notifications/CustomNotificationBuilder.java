// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Build;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationCompat.Action;
import android.text.format.DateFormat;
import android.view.View;
import android.widget.RemoteViews;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;

import javax.annotation.Nullable;

/**
 * Builds a notification using the given inputs. Uses RemoteViews to provide a custom layout.
 */
public class CustomNotificationBuilder implements NotificationBuilder {
    /**
     * Maximum length of CharSequence inputs to prevent excessive memory consumption. At current
     * screen sizes we display about 500 characters at most, so this is a pretty generous limit, and
     * it matches what NotificationCompat does.
     */
    @VisibleForTesting static final int MAX_CHARSEQUENCE_LENGTH = 5 * 1024;

    /**
     * The maximum number of action buttons. One is for the settings button, and two more slots are
     * for developer provided buttons.
     */
    private static final int MAX_ACTION_BUTTONS = 3;

    private final Context mContext;

    private CharSequence mTitle;
    private CharSequence mBody;
    private CharSequence mOrigin;
    private CharSequence mTickerText;
    private Bitmap mLargeIcon;
    private int mSmallIconId;
    private PendingIntent mContentIntent;
    private PendingIntent mDeleteIntent;
    private List<Action> mActions = new ArrayList<>(MAX_ACTION_BUTTONS);
    private int mDefaults = Notification.DEFAULT_ALL;
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

        if (!mActions.isEmpty()) {
            bigView.setViewVisibility(R.id.button_divider, View.VISIBLE);
            bigView.setViewVisibility(R.id.buttons, View.VISIBLE);
            for (Action action : mActions) {
                RemoteViews button = new RemoteViews(
                        mContext.getPackageName(), R.layout.web_notification_button);
                button.setTextViewCompoundDrawablesRelative(R.id.button, action.getIcon(), 0, 0, 0);
                button.setTextViewText(R.id.button, action.getTitle());
                button.setOnClickPendingIntent(R.id.button, action.getActionIntent());
                bigView.addView(R.id.buttons, button);
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            compactView.setViewVisibility(R.id.small_icon_overlay, View.VISIBLE);
            bigView.setViewVisibility(R.id.small_icon_overlay, View.VISIBLE);
        } else {
            compactView.setViewVisibility(R.id.small_icon_footer, View.VISIBLE);
            bigView.setViewVisibility(R.id.small_icon_footer, View.VISIBLE);
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
    public NotificationBuilder setTitle(CharSequence title) {
        mTitle = limitLength(title);
        return this;
    }

    @Override
    public NotificationBuilder setBody(CharSequence body) {
        mBody = limitLength(body);
        return this;
    }

    @Override
    public NotificationBuilder setOrigin(CharSequence origin) {
        mOrigin = limitLength(origin);
        return this;
    }

    @Override
    public NotificationBuilder setTicker(CharSequence tickerText) {
        mTickerText = limitLength(tickerText);
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
        if (mActions.size() == MAX_ACTION_BUTTONS) {
            throw new IllegalStateException(
                    "Cannot add more than " + MAX_ACTION_BUTTONS + " actions.");
        }
        mActions.add(new Action(iconId, limitLength(title), intent));
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

    @Nullable
    private static CharSequence limitLength(@Nullable CharSequence input) {
        if (input == null) {
            return input;
        }
        if (input.length() > MAX_CHARSEQUENCE_LENGTH) {
            return input.subSequence(0, MAX_CHARSEQUENCE_LENGTH);
        }
        return input;
    }
}
