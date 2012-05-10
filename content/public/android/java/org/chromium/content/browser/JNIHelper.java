// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Bitmap;
import android.graphics.RectF;

import java.util.HashSet;
import java.util.Set;

import org.chromium.base.CalledByNative;

class JNIHelper {
    // Holder for content backing store bitmaps of all ChromeViews.  Note that
    // when in software mode, this will be one of the main consumers of RAM in
    // the Java code.  This data structure exists solely to prevent garbage
    // collection and expose the allocations to the DDMS tool.  The backing
    // stores are actually used in C++ with weak global references.
    private static final Set<Bitmap> mBitmapHolder = new HashSet<Bitmap>();

    @CalledByNative
    private static Bitmap createJavaBitmap(int w, int h, boolean keep) {
        Bitmap newBitmap =
            Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);

        if (keep) {
          mBitmapHolder.add(newBitmap);
        }

        return newBitmap;
    }

    @CalledByNative
    private static void deleteJavaBitmap(Bitmap b) {
      mBitmapHolder.remove(b);
      b.recycle();
    }

    // Statics used transiently in paintJavaBitmapToJavaBitmap, but retained to reduce
    // heap churn on each call to that method.
    static private Rect sSourceRect;
    static private RectF sDestRect;
    static private Canvas sCanvas;
    static private Bitmap sNullBitmap;
    static private Paint sPaint;

    @CalledByNative
    public static void paintJavaBitmapToJavaBitmap(
            Bitmap sourceBitmap,
            int sourceX, int sourceY, int sourceBottom, int sourceRight,
            Bitmap destBitmap,
            float destX, float destY, float destBottom, float destRight) {
        if (sCanvas == null) {
            sSourceRect = new Rect();
            sDestRect = new RectF();
            sCanvas = new Canvas();
            sNullBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
            sPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
        }
        sSourceRect.set(sourceX, sourceY, sourceRight, sourceBottom);
        sDestRect.set(destX, destY, destRight, destBottom);
        sCanvas.setBitmap(destBitmap);
        sCanvas.drawBitmap(sourceBitmap, sSourceRect, sDestRect, sPaint);
        sCanvas.setBitmap(sNullBitmap);  // To release |destBitmap| reference.
    }
}
