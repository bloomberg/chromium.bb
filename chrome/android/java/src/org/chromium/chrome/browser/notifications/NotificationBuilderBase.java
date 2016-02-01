// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.graphics.Bitmap;
import android.support.v4.app.NotificationCompat.Action;

import org.chromium.base.VisibleForTesting;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.annotation.Nullable;

/**
 * Abstract base class for building a notification. Stores all given arguments for later use.
 */
public abstract class NotificationBuilderBase {
    /**
     * Maximum length of CharSequence inputs to prevent excessive memory consumption. At current
     * screen sizes we display about 500 characters at most, so this is a pretty generous limit, and
     * it matches what the Notification class does.
     */
    @VisibleForTesting
    static final int MAX_CHARSEQUENCE_LENGTH = 5 * 1024;

    /**
     * The maximum number of action buttons. One is for the settings button, and two more slots are
     * for developer provided buttons.
     */
    private static final int MAX_ACTION_BUTTONS = 3;

    protected CharSequence mTitle;
    protected CharSequence mBody;
    protected CharSequence mOrigin;
    protected CharSequence mTickerText;
    protected Bitmap mLargeIcon;
    protected int mSmallIconId;
    protected PendingIntent mContentIntent;
    protected PendingIntent mDeleteIntent;
    protected List<Action> mActions = new ArrayList<>(MAX_ACTION_BUTTONS);
    protected Action mSettingsAction;
    protected int mDefaults = Notification.DEFAULT_ALL;
    protected long[] mVibratePattern;
    protected long mTimestamp;

    /**
     * Combines all of the options that have been set and returns a new Notification object.
     */
    public abstract Notification build();

    /**
     * Sets the title text of the notification.
     */
    public NotificationBuilderBase setTitle(@Nullable CharSequence title) {
        mTitle = limitLength(title);
        return this;
    }

    /**
     * Sets the body text of the notification.
     */
    public NotificationBuilderBase setBody(@Nullable CharSequence body) {
        mBody = limitLength(body);
        return this;
    }

    /**
     * Sets the origin text of the notification.
     */
    public NotificationBuilderBase setOrigin(@Nullable CharSequence origin) {
        mOrigin = limitLength(origin);
        return this;
    }

    /**
     * Sets the text that is displayed in the status bar when the notification first arrives.
     */
    public NotificationBuilderBase setTicker(@Nullable CharSequence tickerText) {
        mTickerText = limitLength(tickerText);
        return this;
    }

    /**
     * Sets the large icon that is shown in the notification.
     */
    public NotificationBuilderBase setLargeIcon(@Nullable Bitmap icon) {
        mLargeIcon = icon;
        return this;
    }

    /**
     * Sets the the small icon that is shown in the notification and in the status bar.
     */
    public NotificationBuilderBase setSmallIcon(int iconId) {
        mSmallIconId = iconId;
        return this;
    }

    /**
     * Sets the PendingIntent to send when the notification is clicked.
     */
    public NotificationBuilderBase setContentIntent(@Nullable PendingIntent intent) {
        mContentIntent = intent;
        return this;
    }

    /**
     * Sets the PendingIntent to send when the notification is cleared by the user directly from the
     * notification panel.
     */
    public NotificationBuilderBase setDeleteIntent(@Nullable PendingIntent intent) {
        mDeleteIntent = intent;
        return this;
    }

    /**
     * Adds an action to the notification. Actions are typically displayed as a button adjacent to
     * the notification content.
     */
    public NotificationBuilderBase addAction(
            int iconId, @Nullable CharSequence title, @Nullable PendingIntent intent) {
        if (mActions.size() == MAX_ACTION_BUTTONS) {
            throw new IllegalStateException(
                    "Cannot add more than " + MAX_ACTION_BUTTONS + " actions.");
        }
        mActions.add(new Action(iconId, limitLength(title), intent));
        return this;
    }

    /**
     * Adds an action to the notification for opening the settings screen.
     */
    public NotificationBuilderBase addSettingsAction(
            int iconId, @Nullable CharSequence title, @Nullable PendingIntent intent) {
        mSettingsAction = new Action(iconId, limitLength(title), intent);
        return this;
    }

    /**
     * Sets the default notification options that will be used.
     * <p>
     * The value should be one or more of the following fields combined with
     * bitwise-or:
     * {@link Notification#DEFAULT_SOUND}, {@link Notification#DEFAULT_VIBRATE},
     * {@link Notification#DEFAULT_LIGHTS}.
     * <p>
     * For all default values, use {@link Notification#DEFAULT_ALL}.
     */
    public NotificationBuilderBase setDefaults(int defaults) {
        mDefaults = defaults;
        return this;
    }

    /**
     * Sets the vibration pattern to use.
     */
    public NotificationBuilderBase setVibrate(long[] pattern) {
        mVibratePattern = Arrays.copyOf(pattern, pattern.length);
        return this;
    }

    /**
     * Sets the timestamp at which the event of the notification took place.
     */
    public NotificationBuilderBase setTimestamp(long timestamp) {
        mTimestamp = timestamp;
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
