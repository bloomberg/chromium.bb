// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationCompat.Action;
import android.text.format.DateFormat;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.widget.RemoteViews;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.ui.base.LocalizationUtils;

import java.util.Date;

/**
 * Builds a notification using the given inputs. Uses RemoteViews to provide a custom layout.
 */
public class CustomNotificationBuilder extends NotificationBuilderBase {
    /**
     * The maximum number of lines of body text for the expanded state. Fewer lines are used when
     * the text is scaled up, with a minimum of one line.
     */
    private static final int MAX_BODY_LINES = 7;

    /**
     * The fontScale considered large for the purposes of layout.
     */
    private static final float FONT_SCALE_LARGE = 1.3f;

    /**
     * The maximum amount of padding (in dip units) that is applied around views that must have a
     * flexible amount of padding. If the font size is scaled up the applied padding will be scaled
     * down towards 0.
     */
    private static final int MAX_SCALABLE_PADDING_DP = 3;

    /**
     * The amount of padding at the start of the button, either before an icon or before the text.
     */
    private static final int BUTTON_PADDING_START_DP = 8;

    /**
     * The amount of padding between the icon and text of a button. Used only if there is an icon.
     */
    private static final int BUTTON_ICON_PADDING_DP = 8;

    /**
     * The size of the work profile badge (width and height).
     */
    private static final int WORK_PROFILE_BADGE_SIZE_DP = 16;

    /**
     * Material Grey 600 - to be applied to action button icons in the Material theme.
     */
    private static final int BUTTON_ICON_COLOR_MATERIAL = 0xff757575;

    private final Context mContext;

    public CustomNotificationBuilder(Context context) {
        mContext = context;
    }

    @Override
    public Notification build() {
        // A note about RemoteViews and updating notifications. When a notification is passed to the
        // {@code NotificationManager} with the same tag and id as a previous notification, an
        // in-place update will be performed. In that case, the actions of all new
        // {@link RemoteViews} will be applied to the views of the old notification. This is safe
        // for actions that overwrite old values such as setting the text of a {@code TextView}, but
        // care must be taken for additive actions. Especially in the case of
        // {@link RemoteViews#addView} the result could be to append new views below stale ones. In
        // that case {@link RemoteViews#removeAllViews} must be called before adding new ones.
        RemoteViews compactView =
                new RemoteViews(mContext.getPackageName(), R.layout.web_notification);
        RemoteViews bigView =
                new RemoteViews(mContext.getPackageName(), R.layout.web_notification_big);

        float fontScale = mContext.getResources().getConfiguration().fontScale;
        bigView.setInt(R.id.body, "setMaxLines", calculateMaxBodyLines(fontScale));
        int scaledPadding =
                calculateScaledPadding(fontScale, mContext.getResources().getDisplayMetrics());
        String time = DateFormat.getTimeFormat(mContext).format(new Date());
        for (RemoteViews view : new RemoteViews[] {compactView, bigView}) {
            view.setTextViewText(R.id.time, time);
            view.setTextViewText(R.id.title, mTitle);
            view.setTextViewText(R.id.body, mBody);
            view.setTextViewText(R.id.origin, mOrigin);
            view.setImageViewBitmap(R.id.icon, mLargeIcon);
            view.setViewPadding(R.id.title, 0, scaledPadding, 0, 0);
            view.setViewPadding(R.id.body_container, 0, scaledPadding, 0, scaledPadding);
            addWorkProfileBadge(view);
        }
        addActionButtons(bigView);
        configureSettingsButton(bigView);

        if (useMaterial()) {
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
        builder.setWhen(mTimestamp);
        builder.setOnlyAlertOnce(!mRenotify);
        builder.setContent(compactView);

        // Some things are duplicated in the builder to ensure the notification shows correctly on
        // Wear devices and custom lock screens.
        builder.setContentTitle(mTitle);
        builder.setContentText(mBody);
        builder.setSubText(mOrigin);
        builder.setLargeIcon(mLargeIcon);
        for (Action action : mActions) {
            builder.addAction(action);
        }
        if (mSettingsAction != null) {
            builder.addAction(mSettingsAction);
        }

        Notification notification = builder.build();
        notification.bigContentView = bigView;
        return notification;
    }

    /**
     * If there are actions, shows the button related views, and adds a button for each action.
     */
    private void addActionButtons(RemoteViews bigView) {
        // Remove the existing buttons in case an existing notification is being updated.
        bigView.removeAllViews(R.id.buttons);

        // Always set the visibility of the views associated with the action buttons. The current
        // visibility state is not known as perhaps an existing notification is being updated.
        int visibility = mActions.isEmpty() ? View.GONE : View.VISIBLE;
        bigView.setViewVisibility(R.id.button_divider, visibility);
        bigView.setViewVisibility(R.id.buttons, visibility);

        Resources resources = mContext.getResources();
        DisplayMetrics metrics = resources.getDisplayMetrics();
        for (Action action : mActions) {
            RemoteViews view =
                    new RemoteViews(mContext.getPackageName(), R.layout.web_notification_button);

            if (action.getIcon() != 0) {
                // TODO(mvanouwerkerk): If the icon can be provided by web developers, limit its
                // dimensions and decide whether or not to paint it.
                if (useMaterial()) {
                    view.setInt(R.id.button_icon, "setColorFilter", BUTTON_ICON_COLOR_MATERIAL);
                }
                view.setImageViewResource(R.id.button_icon, action.getIcon());

                // Set the padding of the button so the text does not overlap with the icon. Flip
                // between left and right manually as RemoteViews does not expose a method that sets
                // padding in a writing-direction independent way.
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inJustDecodeBounds = true;
                BitmapFactory.decodeResource(resources, action.getIcon(), options);
                int buttonPadding =
                        dpToPx(BUTTON_PADDING_START_DP + BUTTON_ICON_PADDING_DP, metrics)
                        + options.outWidth;
                int buttonPaddingLeft = LocalizationUtils.isLayoutRtl() ? 0 : buttonPadding;
                int buttonPaddingRight = LocalizationUtils.isLayoutRtl() ? buttonPadding : 0;
                view.setViewPadding(R.id.button, buttonPaddingLeft, 0, buttonPaddingRight, 0);
            }

            view.setTextViewText(R.id.button, action.getTitle());
            view.setOnClickPendingIntent(R.id.button, action.getActionIntent());
            bigView.addView(R.id.buttons, view);
        }
    }

    private void configureSettingsButton(RemoteViews bigView) {
        if (mSettingsAction == null) {
            return;
        }
        bigView.setOnClickPendingIntent(R.id.origin, mSettingsAction.getActionIntent());
        if (useMaterial()) {
            bigView.setInt(R.id.origin_settings_icon, "setColorFilter", BUTTON_ICON_COLOR_MATERIAL);
        }
    }

    /**
     * Shows the work profile badge if it is needed.
     */
    private void addWorkProfileBadge(RemoteViews view) {
        Resources resources = mContext.getResources();
        DisplayMetrics metrics = resources.getDisplayMetrics();
        int size = dpToPx(WORK_PROFILE_BADGE_SIZE_DP, metrics);
        int[] colors = new int[size * size];

        // Create an immutable bitmap, so that it can not be reused for painting a badge into it.
        Bitmap bitmap = Bitmap.createBitmap(colors, size, size, Bitmap.Config.ARGB_8888);

        Drawable inputDrawable = new BitmapDrawable(resources, bitmap);
        Drawable outputDrawable = ApiCompatibilityUtils.getUserBadgedDrawableForDensity(
                mContext, inputDrawable, null /* badgeLocation */, metrics.densityDpi);

        // The input bitmap is immutable, so the output drawable will be a different instance from
        // the input drawable if the work profile badge was applied.
        if (inputDrawable != outputDrawable && outputDrawable instanceof BitmapDrawable) {
            view.setImageViewBitmap(
                    R.id.work_profile_badge, ((BitmapDrawable) outputDrawable).getBitmap());
            view.setViewVisibility(R.id.work_profile_badge, View.VISIBLE);
        }
    }

    /**
     * Scales down the maximum number of displayed lines in the body text if font scaling is greater
     * than 1.0. Never scales up the number of lines, as on some devices the notification text is
     * rendered in dp units (which do not scale) and additional lines could lead to cropping at the
     * bottom of the notification.
     *
     * @param fontScale The current system font scaling factor.
     * @return The number of lines to be displayed.
     */
    @VisibleForTesting
    static int calculateMaxBodyLines(float fontScale) {
        if (fontScale > 1.0f) {
            return (int) Math.round(Math.ceil((1 / fontScale) * MAX_BODY_LINES));
        }
        return MAX_BODY_LINES;
    }

    /**
     * Scales down the maximum amount of flexible padding to use if font scaling is over 1.0. Never
     * scales up the amount of padding, as on some devices the notification text is rendered in dp
     * units (which do not scale) and additional padding could lead to cropping at the bottom of the
     * notification. Never scales the padding below zero.
     *
     * @param fontScale The current system font scaling factor.
     * @param displayMetrics The display metrics for the current context.
     * @return The amount of padding to be used, in pixels.
     */
    @VisibleForTesting
    static int calculateScaledPadding(float fontScale, DisplayMetrics displayMetrics) {
        float paddingScale = 1.0f;
        if (fontScale > 1.0f) {
            fontScale = Math.min(fontScale, FONT_SCALE_LARGE);
            paddingScale = (FONT_SCALE_LARGE - fontScale) / (FONT_SCALE_LARGE - 1.0f);
        }
        return dpToPx(paddingScale * MAX_SCALABLE_PADDING_DP, displayMetrics);
    }

    /**
     * Converts a dp value to a px value.
     */
    private static int dpToPx(float value, DisplayMetrics displayMetrics) {
        return Math.round(
                TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, displayMetrics));
    }

    /**
     * Whether to use the Material look and feel or fall back to Holo.
     */
    private static boolean useMaterial() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
    }
}
