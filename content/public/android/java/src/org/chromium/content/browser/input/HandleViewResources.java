// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Helper class for retrieving resources related to selection handles.
 */
@JNINamespace("content")
public class HandleViewResources {
    private static final int[] LEFT_HANDLE_ATTRS = {
        android.R.attr.textSelectHandleLeft,
    };

    private static final int[] CENTER_HANDLE_ATTRS = {
        android.R.attr.textSelectHandle,
    };

    private static final int[] RIGHT_HANDLE_ATTRS = {
        android.R.attr.textSelectHandleRight,
    };

    public static Drawable getLeftHandleDrawable(Context context) {
        return getHandleDrawable(context, LEFT_HANDLE_ATTRS);
    }

    public static Drawable getCenterHandleDrawable(Context context) {
        return getHandleDrawable(context, CENTER_HANDLE_ATTRS);
    }

    public static Drawable getRightHandleDrawable(Context context) {
        return getHandleDrawable(context, RIGHT_HANDLE_ATTRS);
    }

    private static Drawable getHandleDrawable(Context context, final int[] attrs) {
        TypedArray a = context.getTheme().obtainStyledAttributes(attrs);
        Drawable drawable = a.getDrawable(0);
        a.recycle();
        return drawable;
    }

    private static Bitmap getHandleBitmap(Context context, final int[] attrs) {
        // TODO(jdduke): Properly derive and apply theme color.
        TypedArray a = context.getTheme().obtainStyledAttributes(attrs);
        final int resId = a.getResourceId(a.getIndex(0), 0);
        final Resources res = a.getResources();
        a.recycle();

        final Bitmap.Config config = Bitmap.Config.ARGB_8888;
        final BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = false;
        options.inPreferredConfig = config;
        final Bitmap bitmap = BitmapFactory.decodeResource(res, resId, options);
        if (bitmap != null) return bitmap;

        Drawable drawable = getHandleDrawable(context, attrs);
        assert drawable != null;

        final int width = drawable.getIntrinsicWidth();
        final int height = drawable.getIntrinsicHeight();
        Bitmap canvasBitmap = Bitmap.createBitmap(width, height, config);
        Canvas canvas = new Canvas(canvasBitmap);
        drawable.setBounds(0, 0, width, height);
        drawable.draw(canvas);
        return canvasBitmap;
    }

    @CalledByNative
    private static Bitmap getLeftHandleBitmap(Context context) {
        return getHandleBitmap(context, LEFT_HANDLE_ATTRS);
    }

    @CalledByNative
    private static Bitmap getCenterHandleBitmap(Context context) {
        return getHandleBitmap(context, CENTER_HANDLE_ATTRS);
    }

    @CalledByNative
    private static Bitmap getRightHandleBitmap(Context context) {
        return getHandleBitmap(context, RIGHT_HANDLE_ATTRS);
    }
}
