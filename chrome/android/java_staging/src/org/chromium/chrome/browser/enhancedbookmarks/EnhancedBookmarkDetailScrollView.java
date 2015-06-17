// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ScrollView;

import org.chromium.chrome.R;

/**
 * Customized ScrollView used in EnhancedBookmarkDetailDialog. This ScrollView broadcasts scroll
 * change events and computes height compensation value used in detail dialog.
 */
public class EnhancedBookmarkDetailScrollView extends ScrollView {

    /**
     * Listener for scroll change event of the scrollView.
     */
    public interface OnScrollListener {
        void onScrollChanged(int y, int oldY);
    }

    private int mHeightCompensation;
    // Maximum scroll amount by pixels in Y direction (height).
    private int mMaximumScrollY;
    private OnScrollListener mScrollListener;

    public EnhancedBookmarkDetailScrollView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mHeightCompensation =
                getResources().getDimensionPixelSize(R.dimen.enhanced_bookmark_detail_image_height)
                - getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
    }

    /**
     * Sets scroll listener that is called whenever scrollY changes.
     */
    public void setOnScrollListener(OnScrollListener listener) {
        mScrollListener = listener;
    }

    /**
     * Gets height compensation, a fixed length that scroll view uses to extend its length if
     * content is not long enough.
     */
    public int getHeightCompensation() {
        return mHeightCompensation;
    }

    /**
     * Gets the maximum value scrollY can be. MaximumScrollY should never be less than
     * HeightCompensation.
     */
    public int getMaximumScrollY() {
        return mMaximumScrollY;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int childHeight = getChildAt(0).getMeasuredHeight();
        mMaximumScrollY = childHeight > getMeasuredHeight() ? childHeight - getMeasuredHeight() : 0;
    }

    @Override
    protected void onScrollChanged(int x, int y, int oldx, int oldy) {
        if (mScrollListener == null) return;
        mScrollListener.onScrollChanged(y, oldy);
    }
}
