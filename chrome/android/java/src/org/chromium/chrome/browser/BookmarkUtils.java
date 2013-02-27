// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import org.chromium.chrome.R;

/**
 * Util class for bookmarks.
 */
public class BookmarkUtils {

    // There is no public string defining this intent so if Home changes the value, we
    // have to update this string.
    private static final String INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";
    private static final int DEFAULT_RGB_VALUE = 145;
    private static final String TAG = "BookmarkUtils";
    public static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";
    private static final int INSET_DIMENSION_FOR_TOUCHICON = 1;
    private static final int SDK_VERSION_FOR_ACCESS_TO_METHODS = 15;

    /**
     * Creates an intent that will add a shortcut to the home screen.
     * @param context Context used to create the intent.
     * @param url Url of the bookmark.
     * @param title Title of the bookmark.
     * @param favicon Bookmark favicon.
     * @param rValue Red component of the dominant favicon color.
     * @param gValue Green component of the dominant favicon color.
     * @param bValue Blue component of the dominant favicon color.
     * @return Intent for the shortcut.
     */
    public static Intent createAddToHomeIntent(Context context, String url, String title,
            Bitmap favicon, int rValue, int gValue, int bValue) {
        Intent i = new Intent(INSTALL_SHORTCUT);
        Intent shortcutIntent = createShortcutIntent(context, url);
        i.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        i.putExtra(Intent.EXTRA_SHORTCUT_NAME, title);
        i.putExtra(Intent.EXTRA_SHORTCUT_ICON, createIcon(context, favicon, rValue,
                gValue, bValue));
        return i;
    }

    /**
     * Shortcut intent for icon on homescreen.
     * @param context Context used to create the intent.
     * @param url Url of the bookmark.
     * @return Intent for onclick action of the shortcut.
     */
    private static Intent createShortcutIntent(Context context, String url) {
        Intent shortcutIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        shortcutIntent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        return shortcutIntent;
    }

    /**
     * Creates an icon to be associated with this bookmark. If available, the touch icon
     * will be used, else we draw our own.
     * @param context Context used to create the intent.
     * @param favicon Bookmark favicon bitmap.
     * @param rValue Red component of the dominant favicon color.
     * @param gValue Green component of the dominant favicon color.
     * @param bValue Blue component of the dominant favicon color.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    private static Bitmap createIcon(Context context, Bitmap favicon, int rValue,
            int gValue, int bValue) {
        Bitmap bitmap = null;
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int iconDimension = am.getLauncherLargeIconSize();
        final int iconDensity = am.getLauncherLargeIconDensity();
        try {
            bitmap = Bitmap.createBitmap(iconDimension, iconDimension, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            Rect iconBounds = new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight());
            if (favicon == null) {
                favicon = getBitmapFromResourceId(context, R.drawable.globe_favicon, iconDensity);
                rValue = gValue = bValue = DEFAULT_RGB_VALUE;
            }
            final int smallestSide = iconDimension;
            if (favicon.getWidth() >= smallestSide / 2 && favicon.getHeight() >= smallestSide / 2) {
                drawTouchIconToCanvas(context, favicon, canvas, iconBounds);
            } else {
                Bitmap icon = getIconBackground(context, iconDensity);
                if (icon != null) {
                    canvas.drawBitmap(icon, null, iconBounds, new Paint(Paint.ANTI_ALIAS_FLAG));
                }
                drawFaviconToCanvas(context, favicon, canvas, iconBounds, rValue, gValue, bValue);
            }
            canvas.setBitmap(null);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "OutOfMemoryError while trying to draw bitmap on canvas.");
        }
        return bitmap;
    }

    /**
     * Get the icon background asset.
     * @param context Context used to create the intent.
     * @return Bitmap favicon background asset.
     */
    private static Bitmap getIconBackground(Context context, int density) {
        return getBitmapFromResourceId(context, R.mipmap.homescreen_bg, density);
    }

    private static Bitmap getBitmapFromResourceId(Context context, int id, int density) {
        Drawable drawable = null;
        if (Build.VERSION.SDK_INT >= SDK_VERSION_FOR_ACCESS_TO_METHODS) {
            drawable = context.getResources().getDrawableForDensity(id, density);
        } else {
            drawable = context.getResources().getDrawable(id);
        }
        if (drawable instanceof BitmapDrawable) {
            BitmapDrawable bd = (BitmapDrawable) drawable;
            return bd.getBitmap();
        }
        assert false : "The drawable was not a bitmap drawable as expected";
        return null;
    }

    /**
     * Use touch-icon or higher-resolution favicon and round the corners.
     * @param context    Context used to get resources.
     * @param touchIcon  Touch icon bitmap.
     * @param canvas     Canvas that holds the touch icon.
     * @param iconBounds Rectangle bounds needed for adding rounded corners of the touch icon.
     */
    private static void drawTouchIconToCanvas(
            Context context, Bitmap touchIcon, Canvas canvas, Rect iconBounds) {
        Rect src = new Rect(0, 0, touchIcon.getWidth(), touchIcon.getHeight());
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setFilterBitmap(true);
        canvas.drawBitmap(touchIcon, src, iconBounds, paint);
        int borderRadii =
                context.getResources().getDimensionPixelSize(R.dimen.touchicon_border_radii);
        Path path = new Path();
        path.setFillType(Path.FillType.INVERSE_WINDING);
        RectF rect = new RectF(iconBounds);
        rect.inset(INSET_DIMENSION_FOR_TOUCHICON, INSET_DIMENSION_FOR_TOUCHICON);
        path.addRoundRect(rect, borderRadii, borderRadii, Path.Direction.CW);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        canvas.drawPath(path, paint);
    }

    /**
     * Draw the favicon with dominant color.
     * @param context Context used to create the intent.
     * @param favicon favicon bitmap.
     * @param canvas Canvas that holds the favicon.
     * @param iconBounds Rectangle bounds needed to create the homescreen favicon.
     * @param rValue Red component of the dominant favicon color.
     * @param gValue Green component of the dominant favicon color.
     * @param bValue Blue component of the dominant favicon color.
     */
    private static void drawFaviconToCanvas(Context context, Bitmap favicon,
            Canvas canvas, Rect iconBounds, int rValue, int gValue, int bValue) {
        int colorStripWidth = context.getResources().getDimensionPixelSize(
                R.dimen.favicon_colorstrip_width);
        int colorStripHeight = context.getResources().getDimensionPixelSize(
                R.dimen.favicon_colorstrip_height);
        int colorStripPadding = context.getResources().getDimensionPixelSize(
                R.dimen.favicon_colorstrip_padding);
        int colorStripCornerRadii = context.getResources().getDimensionPixelSize(
                R.dimen.favicon_colorstrip_corner_radii);
        int foldSize = context.getResources().getDimensionPixelSize(R.dimen.favicon_fold_size);
        int foldCornerRadii = context.getResources().getDimensionPixelSize(
                R.dimen.favicon_fold_corner_radii);
        int foldBorder = context.getResources().getDimensionPixelSize(R.dimen.favicon_fold_border);
        int foldShadow = context.getResources().getDimensionPixelSize(R.dimen.favicon_fold_shadow);
        int faviconSize = context.getResources().getDimensionPixelSize(R.dimen.favicon_size);

        float x1 = iconBounds.exactCenterX() - (colorStripWidth / 2.0f);
        float x2 = x1 + colorStripWidth;
        float y1 = iconBounds.height() - colorStripPadding;
        float y2 = y1 - colorStripHeight;
        int faviconColorAlpha100 = Color.argb(255, rValue, gValue, bValue);
        Paint stripPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        stripPaint.setColor(faviconColorAlpha100);
        stripPaint.setStyle(Paint.Style.FILL);
        Path stripPath = new Path();
        stripPath.moveTo(x1, y2);
        stripPath.lineTo(x2, y2);
        stripPath.lineTo(x2, y1 - colorStripCornerRadii);
        stripPath.arcTo(new RectF(x2 - colorStripCornerRadii, y1 - colorStripCornerRadii, x2, y1),
                0, 90);
        stripPath.lineTo(x1 + colorStripCornerRadii, y1);
        stripPath.arcTo(new RectF(x1, y1 - colorStripCornerRadii, x1 + colorStripCornerRadii, y1),
                90, 90);
        stripPath.lineTo(x1, y2);
        canvas.drawPath(stripPath, stripPaint);

        float ax, ay, bx, by, cx, cy;
        bx = ax = x2 - foldSize;
        cx = x2;
        ay = colorStripPadding;
        by = cy = colorStripPadding + foldSize;
        int blackAlpha10 = Color.argb((int) (255 * 0.10f), 0, 0, 0);

        Paint foldShadowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        foldShadowPaint.setColor(Color.WHITE);
        foldShadowPaint.setShadowLayer(1, 0, foldShadow, blackAlpha10);
        int faviconColorAlpha60 = Color.argb((int) (255 * 0.60f), rValue, gValue, bValue);
        Paint foldGradientPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        foldGradientPaint.setShader(new LinearGradient(x2 - foldSize / 2.0f,
                colorStripPadding + foldSize / 2.0f, x2 - foldSize, colorStripPadding + foldSize,
                faviconColorAlpha60, faviconColorAlpha100,
                android.graphics.Shader.TileMode.CLAMP));
        Path foldPath = new Path();
        foldPath.moveTo(ax, ay);
        foldPath.lineTo(cx, cy);
        foldPath.lineTo(bx + foldCornerRadii, by);
        foldPath.arcTo(new RectF(bx, by - foldCornerRadii, bx + foldCornerRadii, by), 90, 90);
        foldPath.lineTo(ax, ay);
        canvas.drawPath(foldPath, foldShadowPaint);
        canvas.drawPath(foldPath, foldGradientPaint);
        int faviconColorAlpha80 = Color.argb((int) (255 * 0.80f), rValue, gValue, bValue);
        Paint foldBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        foldBorderPaint.setColor(faviconColorAlpha80);
        foldBorderPaint.setStyle(Paint.Style.STROKE);
        foldBorderPaint.setStrokeWidth(foldBorder);
        canvas.drawPath(foldPath, foldBorderPaint);
        try {
            Bitmap scaledFavicon = Bitmap.createScaledBitmap(favicon, faviconSize, faviconSize,
                    true);
            canvas.drawBitmap(scaledFavicon,
                    iconBounds.exactCenterX() - scaledFavicon.getWidth() / 2.0f,
                    iconBounds.exactCenterY() - scaledFavicon.getHeight() / 2.0f, null);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "OutOfMemoryError while trying to draw bitmap on canvas.");
        }
    }
}
