// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.content.Context;
import android.util.AttributeSet;

/**
 * Helper {@link AsyncImageView} that will force the dimensions of the view to be equal.
 */
public class SquareAsyncImageView extends AsyncImageView {
    /** Creates an instance of {@link SquareAsyncImageView}. */
    public SquareAsyncImageView(Context context) {
        this(context, null, 0);
    }

    /** Creates an instance of {@link SquareAsyncImageView}. */
    public SquareAsyncImageView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    /** Creates an instance of {@link SquareAsyncImageView}. */
    public SquareAsyncImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    // AsyncImageView implementation.
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int measureWidth = MeasureSpec.getSize(widthMeasureSpec);
        int measureHeight = MeasureSpec.getSize(heightMeasureSpec);

        int squareSize;

        // Check if there is a valid explicitly set dimension first (prioritize width).
        if (MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY && measureWidth > 0) {
            squareSize = measureWidth;
        } else if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.EXACTLY
                && measureHeight > 0) {
            squareSize = measureHeight;
        } else {
            squareSize = Math.min(measureWidth, measureHeight);
        }

        int squareMeasureSpec = MeasureSpec.makeMeasureSpec(squareSize, MeasureSpec.EXACTLY);

        super.onMeasure(squareMeasureSpec, squareMeasureSpec);
    }
}