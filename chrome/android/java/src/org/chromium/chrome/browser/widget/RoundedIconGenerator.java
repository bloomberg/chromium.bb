// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.RectF;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.UrlUtilities;

import java.util.Locale;

/**
 * Generator for transparent icons containing a rounded rectangle with a given background color,
 * having a centered character drawn on top of it.
 */
public class RoundedIconGenerator {
    private final int mIconWidthPx;
    private final int mIconHeightPx;
    private final int mCornerRadiusPx;

    private final RectF mBackgroundRect;

    private final Paint mBackgroundPaint;
    private final TextPaint mTextPaint;

    private final float mTextHeight;
    private final float mTextYOffset;

    /**
     * Constructs the generator and initializes the common members based on the display density.
     *
     * @param context The context used for initialization.
     * @param iconWidthDp The width of the generated icon.
     * @param iconHeightDp The height of the generated icon.
     * @param cornerRadiusDp The radius of the corners in the icon.
     * @param backgroundColor Color at which the rounded rectangle should be drawn.
     * @param textSizeSp Size at which the text should be drawn.
     */
    public RoundedIconGenerator(Context context, int iconWidthDp, int iconHeightDp,
                                int cornerRadiusDp, int backgroundColor, int textSizeSp) {
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();

        mIconWidthPx = (int) (iconWidthDp * displayMetrics.density);
        mIconHeightPx = (int) (iconHeightDp * displayMetrics.density);
        mCornerRadiusPx = (int) (cornerRadiusDp * displayMetrics.density);

        mBackgroundRect = new RectF(0, 0, mIconWidthPx, mIconHeightPx);

        mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBackgroundPaint.setColor(backgroundColor);

        mTextPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        mTextPaint.setColor(Color.WHITE);
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setTextSize(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_SP, textSizeSp, displayMetrics));

        FontMetrics textFontMetrics = mTextPaint.getFontMetrics();
        mTextHeight = (float) Math.ceil(textFontMetrics.bottom - textFontMetrics.top);
        mTextYOffset = -textFontMetrics.top;
    }

    /**
     * Generates an icon based on |text|.
     *
     * @param text The text to render the first character of on the icon.
     * @return The generated icon.
     */
    public Bitmap generateIconForText(String text) {
        Bitmap icon = Bitmap.createBitmap(mIconWidthPx, mIconHeightPx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(icon);

        canvas.drawRoundRect(mBackgroundRect, mCornerRadiusPx, mCornerRadiusPx, mBackgroundPaint);

        String displayText = text.substring(0, 1).toUpperCase(Locale.getDefault());
        float textWidth = mTextPaint.measureText(displayText);

        canvas.drawText(
                displayText,
                (mIconWidthPx - textWidth) / 2f,
                Math.round((Math.max(mIconHeightPx, mTextHeight) - mTextHeight)
                        / 2.0f + mTextYOffset),
                mTextPaint);

        return icon;
    }

    /**
     * Returns a Bitmap representing the icon to be used for |url|.
     *
     * @param url URL for which the icon should be generated.
     * @param includePrivateRegistries Should private registries be considered as TLDs?
     * @return The generated icon, or NULL if |url| is empty or the domain cannot be resolved.
     */
    public Bitmap generateIconForUrl(String url, boolean includePrivateRegistries) {
        if (TextUtils.isEmpty(url)) return null;

        String domain = UrlUtilities.getDomainAndRegistry(url, includePrivateRegistries);
        if (TextUtils.isEmpty(domain)) {
            if (url.startsWith(UrlConstants.CHROME_SCHEME)
                    || url.startsWith(UrlConstants.CHROME_NATIVE_SCHEME)) {
                domain = "chrome";
            } else {
                return null;
            }
        }

        return generateIconForText(domain);
    }

    /**
     * Returns a Bitmap representing the icon to be used for |url|. Private registries such
     * as "appspot.com" will not be considered as effective TLDs.
     *
     * @TODO(beverloo) Update all call-sites of rounded icons to be explicit about whether
     * private registries should be considered, matching the getDomainAndRegistry requirements.
     * See https://crbug.com/458104.
     *
     * @param url URL for which the icon should be generated.
     * @return The generated icon, or NULL if |url| is empty or the domain cannot be resolved.
     */
    public Bitmap generateIconForUrl(String url) {
        return generateIconForUrl(url, false);
    }
}
