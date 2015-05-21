// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;

/**
 * ViewGroup that places child views in lines from start to end, wraps items to
 * next line if content too long to hold in one line.
 */
public class FlowLayout extends ViewGroup {
    private static final int SPACING_DEFAULT_DP = 3;

    private int mHorizontalSpacing;
    private int mVerticalSpacing;

    public FlowLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        int spacingDefaultPx = Math.round(
                getResources().getDisplayMetrics().density * SPACING_DEFAULT_DP);
        mHorizontalSpacing = spacingDefaultPx;
        mVerticalSpacing = spacingDefaultPx;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int availableWidth = MeasureSpec.getSize(widthMeasureSpec)
                - ApiCompatibilityUtils.getPaddingEnd(this);
        int widthMode = MeasureSpec.getMode(widthMeasureSpec);

        boolean growHeight = widthMode != MeasureSpec.UNSPECIFIED;

        int width = 0;
        int height = getPaddingTop();

        int currentWidth = ApiCompatibilityUtils.getPaddingStart(this);
        int currentHeight = 0;

        boolean newLine = false;
        int spacing = 0;

        final int count = getChildCount();
        for (int i = 0; i < count; i++) {
            View child = getChildAt(i);
            measureChild(child, widthMeasureSpec, heightMeasureSpec);

            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            spacing = mHorizontalSpacing;

            if (growHeight && currentWidth + child.getMeasuredWidth() > availableWidth) {
                height += currentHeight + mVerticalSpacing;
                currentHeight = 0;
                width = Math.max(width, currentWidth - spacing);
                currentWidth = ApiCompatibilityUtils.getPaddingStart(this);
                newLine = true;
            } else {
                newLine = false;
            }

            lp.start = currentWidth;
            lp.top = height;

            currentWidth += child.getMeasuredWidth() + spacing;
            currentHeight = Math.max(currentHeight, child.getMeasuredHeight());

        }

        if (!newLine) {
            height += currentHeight;
            width = Math.max(width, currentWidth - spacing);
        }

        width += ApiCompatibilityUtils.getPaddingEnd(this);
        height += getPaddingBottom();

        setMeasuredDimension(resolveSize(width, widthMeasureSpec),
                resolveSize(height, heightMeasureSpec));
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // Entire width of layout group
        int width = r - l;
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            int left = isRtl ? width - lp.start - child.getMeasuredWidth() : lp.start;
            child.layout(left, lp.top, left + child.getMeasuredWidth(),
                    lp.top + child.getMeasuredHeight());
        }
    }

    @Override
    protected boolean checkLayoutParams(ViewGroup.LayoutParams p) {
        return p instanceof LayoutParams;
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
    }

    @Override
    public LayoutParams generateLayoutParams(AttributeSet attrs) {
        return new LayoutParams(getContext(), attrs);
    }

    @Override
    protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) {
        return new LayoutParams(p.width, p.height);
    }

    /**
     * LayoutParams for FlowLayout.
     */
    public static class LayoutParams extends ViewGroup.LayoutParams {
        // distance from start of view group to start of the view.
        public int start;
        // vertical coordinate of the view
        public int top;

        public LayoutParams(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public LayoutParams(int w, int h) {
            super(w, h);
        }
    }
}
