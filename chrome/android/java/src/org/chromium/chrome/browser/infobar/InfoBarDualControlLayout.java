// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * Automatically lays out one or two Views for an infobar, placing them on the same row if possible
 * and stacking them otherwise.
 *
 * Use cases of this Layout include placement of infobar buttons and placement of TextViews inside
 * of spinner controls (http://goto.google.com/infobar-spec).
 *
 * Layout parameters (i.e. margins) are ignored to enforce infobar consistency.  Alignment defines
 * where the controls are placed (for RTL, flip everything):
 *
 * ALIGN_START                      ALIGN_APART                      ALIGN_END
 * -----------------------------    -----------------------------    -----------------------------
 * | PRIMARY SECONDARY         |    | SECONDARY         PRIMARY |    |         SECONDARY PRIMARY |
 * -----------------------------    -----------------------------    -----------------------------
 *
 * Controls are stacked automatically when they don't fit on the same row, with each control taking
 * up the full available width and with the primary control sitting on top of the secondary.
 * -----------------------------
 * | PRIMARY------------------ |
 * | SECONDARY---------------- |
 * -----------------------------
 */
public final class InfoBarDualControlLayout extends ViewGroup {
    public static final int ALIGN_START = 0;
    public static final int ALIGN_END = 1;
    public static final int ALIGN_APART = 2;

    private final int mHorizontalMarginBetweenViews;

    private int mAlignment = ALIGN_START;
    private int mStackedMargin;

    private boolean mIsStacked;
    private View mPrimaryView;
    private View mSecondaryView;

    /**
     * Construct a new InfoBarDualControlLayout.
     *
     * See {@link ViewGroup} for parameter details.  attrs may be null if constructed dynamically.
     */
    public InfoBarDualControlLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Cache dimensions.
        Resources resources = getContext().getResources();
        mHorizontalMarginBetweenViews =
                resources.getDimensionPixelSize(R.dimen.infobar_control_margin_between_items);
    }

    /**
     * Define how the controls will be laid out.
     *
     * @param alignment One of ALIGN_START, ALIGN_APART, ALIGN_END.
     */
    void setAlignment(int alignment) {
        mAlignment = alignment;
    }

    /**
     * Sets the margin between the controls when they're stacked.  By default, there is no margin.
     */
    void setStackedMargin(int stackedMargin) {
        mStackedMargin = stackedMargin;
    }

    @Override
    public void onViewAdded(View child) {
        super.onViewAdded(child);

        if (mPrimaryView == null) {
            mPrimaryView = child;
        } else if (mSecondaryView == null) {
            mSecondaryView = child;
        } else {
            throw new IllegalStateException("Too many children added to InfoBarDualControlLayout");
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mIsStacked = false;

        // Measure the primary View, allowing it to be as wide as the Layout.
        int maxWidth = MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED
                ? Integer.MAX_VALUE : MeasureSpec.getSize(widthMeasureSpec);
        int unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        measureChild(mPrimaryView, unspecifiedSpec, unspecifiedSpec);

        int layoutWidth = mPrimaryView.getMeasuredWidth();
        int layoutHeight = mPrimaryView.getMeasuredHeight();

        if (mSecondaryView != null) {
            // Measure the secondary View, allowing it to be as wide as the layout.
            measureChild(mSecondaryView, unspecifiedSpec, unspecifiedSpec);
            int combinedWidth = mPrimaryView.getMeasuredWidth()
                    + mHorizontalMarginBetweenViews + mSecondaryView.getMeasuredWidth();

            if (combinedWidth > maxWidth) {
                // Stack the Views on top of each other.
                mIsStacked = true;

                int widthSpec = MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.EXACTLY);
                mPrimaryView.measure(widthSpec, unspecifiedSpec);
                mSecondaryView.measure(widthSpec, unspecifiedSpec);

                layoutWidth = maxWidth;
                layoutHeight = mPrimaryView.getMeasuredHeight() + mStackedMargin
                        + mSecondaryView.getMeasuredHeight();
            } else {
                // The Views fit side by side.  Check which is taller to find the layout height.
                layoutWidth = combinedWidth;
                layoutHeight = Math.max(layoutHeight, mSecondaryView.getMeasuredHeight());
            }
        }

        setMeasuredDimension(resolveSize(layoutWidth, widthMeasureSpec),
                resolveSize(layoutHeight, heightMeasureSpec));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int width = right - left;
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);
        boolean isPrimaryOnRight = (isRtl && mAlignment == ALIGN_START)
                || (!isRtl && (mAlignment == ALIGN_APART || mAlignment == ALIGN_END));

        int primaryRight = isPrimaryOnRight ? width : mPrimaryView.getMeasuredWidth();
        int primaryLeft = primaryRight - mPrimaryView.getMeasuredWidth();
        int primaryHeight = mPrimaryView.getMeasuredHeight();
        mPrimaryView.layout(primaryLeft, 0, primaryRight, primaryHeight);

        if (mIsStacked) {
            // Fill out the row.  onMeasure() should have already applied the correct width.
            int secondaryTop = primaryHeight + mStackedMargin;
            int secondaryBottom = secondaryTop + mSecondaryView.getMeasuredHeight();
            mSecondaryView.layout(
                    0, secondaryTop, mSecondaryView.getMeasuredWidth(), secondaryBottom);
        } else if (mSecondaryView != null) {
            // Center the secondary View vertically with the primary View.
            int secondaryHeight = mSecondaryView.getMeasuredHeight();
            int primaryCenter = primaryHeight / 2;
            int secondaryTop = primaryCenter - (secondaryHeight / 2);
            int secondaryBottom = secondaryTop + secondaryHeight;

            // Determine where to place the secondary View.
            int secondaryLeft;
            int secondaryRight;
            if (mAlignment == ALIGN_APART) {
                // Put the second View on the other side of the Layout from the primary View.
                secondaryLeft = isPrimaryOnRight ? 0 : width - mSecondaryView.getMeasuredWidth();
                secondaryRight = secondaryLeft + mSecondaryView.getMeasuredWidth();
            } else if (isPrimaryOnRight) {
                // Sit to the left of the primary View.
                secondaryRight = primaryLeft - mHorizontalMarginBetweenViews;
                secondaryLeft = secondaryRight - mSecondaryView.getMeasuredWidth();
            } else {
                // Sit to the right of the primary View.
                secondaryLeft = primaryRight + mHorizontalMarginBetweenViews;
                secondaryRight = secondaryLeft + mSecondaryView.getMeasuredWidth();
            }

            mSecondaryView.layout(
                    secondaryLeft, secondaryTop, secondaryRight, secondaryBottom);
        }
    }
}
