// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.ui.base.LocalizationUtils;

/**
 * Displays a set of stars representing a rating for an app.
 */
public class RatingView extends View {
    private static final int MAX_INCREMENT = 10;

    // Bitmaps used to draw the ratings.
    private final Bitmap mStarFull;
    private final Bitmap mStarHalf;
    private final Bitmap mStarEmpty;

    /** Stores whether or not the layout is left-to-right. */
    private final boolean mIsLayoutLTR;

    /** Variables used for drawing. */
    private final Rect mDrawingRect;

    /** Each increment represents 0.5 stars. */
    private int mIncrements;

    public RatingView(Context context, AttributeSet params) {
        super(context, params);
        mIsLayoutLTR = !LocalizationUtils.isLayoutRtl();
        mDrawingRect = new Rect();

        // Cache the Bitmaps.
        Resources res = getResources();
        Bitmap starHalf = BitmapFactory.decodeResource(res, R.drawable.btn_star_half);
        if (!mIsLayoutLTR) {
            // RTL mode requires flipping the Bitmap.
            int width = starHalf.getWidth();
            int height = starHalf.getHeight();
            Matrix m = new Matrix();
            m.preScale(-1, 1);
            starHalf = Bitmap.createBitmap(starHalf, 0, 0, width, height, m, false);
        }
        mStarHalf = starHalf;
        mStarFull = BitmapFactory.decodeResource(res, R.drawable.btn_star_full);
        mStarEmpty = BitmapFactory.decodeResource(res, R.drawable.btn_star_empty);
    }

    /**
     * Initializes the RatingView.
     * @param rating How many stars to display.
     */
    void initialize(float rating) {
        // Ratings are rounded to the nearest 0.5 increment, like in the Play Store.
        mIncrements = Math.round(rating * 2);
    }

    @Override
    public void onDraw(Canvas canvas) {
        int starSize = canvas.getHeight();

        // Start off on the left for LTR mode, on the right for RTL.
        mDrawingRect.top = 0;
        mDrawingRect.bottom = starSize;
        mDrawingRect.left = mIsLayoutLTR ? 0 : (canvas.getWidth() - starSize);
        mDrawingRect.right = mDrawingRect.left + starSize;

        // Draw all the stars.
        for (int i = 0; i < MAX_INCREMENT; i += 2) {
            Bitmap toShow = mStarEmpty;
            if (i < mIncrements) {
                boolean isFullStar = (mIncrements - i) >= 2;
                toShow = isFullStar ? mStarFull : mStarHalf;
            }
            canvas.drawBitmap(toShow, null, mDrawingRect, null);

            // Scooch over to show the next star.
            mDrawingRect.left += (mIsLayoutLTR ? 1 : -1) * starSize;
            mDrawingRect.right = mDrawingRect.left + starSize;
        }
    }
}
