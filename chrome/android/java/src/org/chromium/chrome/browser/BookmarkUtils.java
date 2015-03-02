// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
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
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

import java.util.List;

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
    private static final int TOUCHICON_BORDER_RADII = 10;
    private static final int GENERATED_ICON_SIZE_DP = 40;
    private static final int GENERATED_ICON_ROUNDED_CORNERS_DP = 2;
    private static final int GENERATED_ICON_FONT_SIZE_DP = 16;

    /**
     * Creates an intent that will add a shortcut to the home screen.
     * @param context Context used to create the intent.
     * @param shortcutIntent Intent to fire when the shortcut is activated.
     * @param title Title of the bookmark.
     * @param favicon Bookmark favicon.
     * @param url URL of the bookmark.
     * @param rValue Red component of the dominant favicon color.
     * @param gValue Green component of the dominant favicon color.
     * @param bValue Blue component of the dominant favicon color.
     * @return Intent for the shortcut.
     */
    public static Intent createAddToHomeIntent(Context context, Intent shortcutIntent, String title,
            Bitmap favicon, String url, int rValue, int gValue, int bValue) {
        Intent i = new Intent(INSTALL_SHORTCUT);
        i.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        i.putExtra(Intent.EXTRA_SHORTCUT_NAME, title);
        i.putExtra(Intent.EXTRA_SHORTCUT_ICON, createIcon(context, favicon, url, rValue,
                gValue, bValue));
        return i;
    }

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
        Intent shortcutIntent = createShortcutIntent(context, url);
        return createAddToHomeIntent(
                context, shortcutIntent, title, favicon, url, rValue, gValue, bValue);
    }

    /**
     * Shortcut intent for icon on homescreen.
     * @param context Context used to create the intent.
     * @param url Url of the bookmark.
     * @return Intent for onclick action of the shortcut.
     */
    public static Intent createShortcutIntent(Context context, String url) {
        Intent shortcutIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        shortcutIntent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        return shortcutIntent;
    }

    /**
     * Utility method to check if a shortcut can be added to the homescreen.
     * @param context Context used to get the package manager.
     * @return if a shortcut can be added to the homescreen under the current profile.
     */
    public static boolean isAddToHomeIntentSupported(Context context) {
        PackageManager pm = context.getPackageManager();
        Intent i = new Intent(INSTALL_SHORTCUT);
        List<ResolveInfo> receivers = pm.queryBroadcastReceivers(
                i, PackageManager.GET_INTENT_FILTERS);
        return !receivers.isEmpty();
    }

    /**
     * Creates an icon to be associated with this bookmark. If available, the touch icon
     * will be used, else we draw our own.
     * @param context Context used to create the intent.
     * @param favicon Bookmark favicon bitmap.
     * @param url URL of the bookmark.
     * @param rValue Red component of the dominant favicon color.
     * @param gValue Green component of the dominant favicon color.
     * @param bValue Blue component of the dominant favicon color.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    private static Bitmap createIcon(Context context, Bitmap favicon, String url, int rValue,
            int gValue, int bValue) {
        Bitmap bitmap = null;
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int iconSize = am.getLauncherLargeIconSize();
        final int iconDensity = am.getLauncherLargeIconDensity();
        try {
            bitmap = Bitmap.createBitmap(iconSize, iconSize, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            if (favicon == null) {
                favicon = getBitmapFromResourceId(context, R.drawable.globe_favicon, iconDensity);
                rValue = gValue = bValue = DEFAULT_RGB_VALUE;
            }
            final int smallestSide = iconSize;
            if (favicon.getWidth() >= smallestSide / 2 && favicon.getHeight() >= smallestSide / 2) {
                drawTouchIconToCanvas(context, favicon, canvas);
            } else {
                drawWidgetBackgroundToCanvas(context, canvas, iconDensity, url,
                        Color.rgb(rValue, gValue, bValue));
            }
            canvas.setBitmap(null);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "OutOfMemoryError while trying to draw bitmap on canvas.");
        }
        return bitmap;
    }

    @TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1)
    private static Bitmap getBitmapFromResourceId(Context context, int id, int density) {
        Drawable drawable = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1) {
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
     */
    private static void drawTouchIconToCanvas(Context context, Bitmap touchIcon, Canvas canvas) {
        Rect iconBounds = new Rect(0, 0, canvas.getWidth(), canvas.getHeight());
        Rect src = new Rect(0, 0, touchIcon.getWidth(), touchIcon.getHeight());
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setFilterBitmap(true);
        canvas.drawBitmap(touchIcon, src, iconBounds, paint);
        // Convert dp to px.
        int borderRadii = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                TOUCHICON_BORDER_RADII, context.getResources().getDisplayMetrics());
        Path path = new Path();
        path.setFillType(Path.FillType.INVERSE_WINDING);
        RectF rect = new RectF(iconBounds);
        rect.inset(INSET_DIMENSION_FOR_TOUCHICON, INSET_DIMENSION_FOR_TOUCHICON);
        path.addRoundRect(rect, borderRadii, borderRadii, Path.Direction.CW);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        canvas.drawPath(path, paint);
    }

    /**
     * Draw document icon to canvas.
     * @param context     Context used to get bitmap resources.
     * @param canvas      Canvas that holds the document icon.
     * @param iconDensity Density information to get bitmap resources.
     * @param url         URL of the bookmark.
     * @param color       Color for the document icon's folding and the bottom strip.
     */
    private static void drawWidgetBackgroundToCanvas(
            Context context, Canvas canvas, int iconDensity, String url, int color) {
        Rect iconBounds = new Rect(0, 0, canvas.getWidth(), canvas.getHeight());
        Bitmap bookmarkWidgetBg =
                getBitmapFromResourceId(context, R.mipmap.bookmark_widget_bg, iconDensity);

        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(bookmarkWidgetBg, null, iconBounds, paint);

        float density = (float) iconDensity / DisplayMetrics.DENSITY_MEDIUM;
        int iconSize = (int) (GENERATED_ICON_SIZE_DP * density);
        int iconRoundedEdge = (int) (GENERATED_ICON_ROUNDED_CORNERS_DP * density);
        int iconFontSize = (int) (GENERATED_ICON_FONT_SIZE_DP * density);

        RoundedIconGenerator generator = new RoundedIconGenerator(
                iconSize, iconSize, iconRoundedEdge, color, iconFontSize);
        Bitmap icon = generator.generateIconForUrl(url);
        if (icon == null) return; // Bookmark URL does not have a domain.
        canvas.drawBitmap(icon, iconBounds.exactCenterX() - icon.getWidth() / 2.0f,
                iconBounds.exactCenterY() - icon.getHeight() / 2.0f, null);
    }
}
