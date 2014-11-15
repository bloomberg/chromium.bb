// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.R;
import org.chromium.ui.base.SystemUIResourceType;

/**
 * Helper class for retrieving both system and embedded UI-related resources.
 */
@JNINamespace("content")
class UIResources {
    private static Bitmap getResourceBitmap(Resources res, int resId, int reqWidth, int reqHeight) {
        final BitmapFactory.Options options = new BitmapFactory.Options();
        if (reqWidth > 0 && reqHeight > 0) {
            options.inJustDecodeBounds = true;
            BitmapFactory.decodeResource(res, resId, options);
            options.inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight);
        }
        options.inJustDecodeBounds = false;
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        return BitmapFactory.decodeResource(res, resId, options);
    }

    // http://developer.android.com/training/displaying-bitmaps/load-bitmap.html
    private static int calculateInSampleSize(
            BitmapFactory.Options options, int reqWidth, int reqHeight) {
        // Raw height and width of image
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > reqHeight || width > reqWidth) {
            // Calculate ratios of height and width to requested height and width
            final int heightRatio = Math.round((float) height / (float) reqHeight);
            final int widthRatio = Math.round((float) width / (float) reqWidth);

            // Choose the smallest ratio as inSampleSize value, this will guarantee
            // a final image with both dimensions larger than or equal to the
            // requested height and width.
            inSampleSize = heightRatio < widthRatio ? heightRatio : widthRatio;
        }

        return inSampleSize;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private static Bitmap getBitmap(
            Context context, int uiResourceId, int reqWidth, int reqHeight) {
        Resources res = null;
        int resId = 0;

        if (uiResourceId == SystemUIResourceType.OVERSCROLL_EDGE) {
            res = Resources.getSystem();
            resId = res.getIdentifier("android:drawable/overscroll_edge", null, null);
        } else if (uiResourceId == SystemUIResourceType.OVERSCROLL_GLOW) {
            res = Resources.getSystem();
            resId = res.getIdentifier("android:drawable/overscroll_glow", null, null);
        } else if (uiResourceId == SystemUIResourceType.OVERSCROLL_REFRESH_IDLE) {
            res = context.getResources();
            resId = R.drawable.refresh_gray;
        } else if (uiResourceId == SystemUIResourceType.OVERSCROLL_REFRESH_ACTIVE) {
            res = context.getResources();
            resId = R.drawable.refresh_blue;
        } else {
            assert false;
        }

        if (res == null) return null;
        if (resId == 0) return null;

        return getResourceBitmap(res, resId, reqWidth, reqHeight);
    }
}
