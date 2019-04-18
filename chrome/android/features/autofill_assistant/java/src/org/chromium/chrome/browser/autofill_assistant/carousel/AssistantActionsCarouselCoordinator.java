// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.content.Context;
import android.support.v7.widget.OrientationHelper;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.LayoutManager;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * A coordinator responsible for suggesting chips to the user. If there is one chip to display, it
 * will be centered on the screen. If there are more than one chip, all but the last one will be
 * displayed from the right of the screen to the left, and the last one will be displayed at the
 * left of the screen.
 *
 * <p>For instance, if chips = [1, 2, 3]:
 * |              |
 * |[3]     [2][1]|
 * |              |
 *
 * <p>If there are too many chips to display all of them on the screen, the carousel will scroll
 * such that the last chips is fixed. For instance, if chips = [1, 2, 3, 4, 5, 6]:
 * |               |
 * |[6][4][3][2][1]|       (before)
 * |               |
 *
 * |               |
 * |[6][5][4][3][2]|       (after horizontal scrolling from left to right)
 * |               |
 */
public class AssistantActionsCarouselCoordinator implements AssistantCarouselCoordinator {
    private final RecyclerView mView;

    public AssistantActionsCarouselCoordinator(Context context, AssistantCarouselModel model) {
        mView = new RecyclerView(context);

        int horizontalPadding = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        int spaceBetweenViewsPx = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_carousel_chips_spacing);

        LayoutManager layoutManager =
                new CustomLayoutManager(spaceBetweenViewsPx, horizontalPadding);
        // Workaround for b/128679161.
        layoutManager.setMeasurementCacheEnabled(false);

        // We set the left padding on the parent as we want the recycler view to clip its children
        // on the left side. The right padding is computed by the layout manager as we don't want
        // children to be clipped when scrolling them to the right. We also have to compensate the
        // padding with the half space that will be added to the chip on the left of the screen.
        mView.setPadding(horizontalPadding - spaceBetweenViewsPx / 2, 0, 0, 0);
        mView.setLayoutManager(layoutManager);

        // TODO(crbug.com/806868): WRAP_CONTENT height should also work.
        mView.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_button_height)));

        mView.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model.getChipsModel(),
                        AssistantChipViewHolder::getViewType, CustomViewHolder::bind),
                CustomViewHolder::create));
    }

    @Override
    public RecyclerView getView() {
        return mView;
    }

    /**
     * Custom ViewHolder that wraps a ButtonView and a rectangular background. Setting a rectangular
     * background behind the chips is necessary as chips are scrolled behind the last (fixed) one
     * and need to be hidden.
     */
    private static class CustomViewHolder extends RecyclerView.ViewHolder {
        private final View mView;
        private final AssistantChipViewHolder mDelegateViewHolder;

        public CustomViewHolder(View view, AssistantChipViewHolder delegateViewHolder) {
            super(view);
            mView = view;
            mDelegateViewHolder = delegateViewHolder;
        }

        static CustomViewHolder create(ViewGroup parent, int viewType) {
            // Wrap the chip inside a rectangular FrameLayout with white background.
            FrameLayout frameLayout = new FrameLayout(parent.getContext());
            frameLayout.setBackgroundColor(
                    ApiCompatibilityUtils.getColor(parent.getContext().getResources(),
                            org.chromium.chrome.R.color.sheet_bg_color));
            frameLayout.setLayoutParams(new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            AssistantChipViewHolder delegateViewHolder =
                    AssistantChipViewHolder.create(frameLayout, viewType);
            ButtonView delegateView = delegateViewHolder.mView;
            frameLayout.addView(delegateView);

            // Center the chip inside the FrameLayout.
            FrameLayout.LayoutParams layoutParams =
                    (FrameLayout.LayoutParams) delegateView.getLayoutParams();
            layoutParams.gravity = Gravity.CENTER_HORIZONTAL;
            delegateView.setLayoutParams(layoutParams);

            return new CustomViewHolder(frameLayout, delegateViewHolder);
        }

        void bind(AssistantChip chip) {
            mDelegateViewHolder.bind(chip);
        }
    }

    // TODO(crbug.com/806868): Handle RTL layouts.
    // TODO(crbug.com/806868): Recycle invisible children instead of laying all of them out.
    private class CustomLayoutManager extends RecyclerView.LayoutManager {
        private final OrientationHelper mOrientationHelper;
        private final int mPaddingEnd;
        private final int mSpaceBetweenChildren;
        private int mMaxScroll;
        private int mScroll;

        private CustomLayoutManager(int spaceBetweenViewsPx, int paddingEndPx) {
            mOrientationHelper = OrientationHelper.createHorizontalHelper(this);
            mSpaceBetweenChildren = spaceBetweenViewsPx;
            mPaddingEnd = paddingEndPx;
        }

        @Override
        public RecyclerView.LayoutParams generateDefaultLayoutParams() {
            return new RecyclerView.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        @Override
        public boolean isAutoMeasureEnabled() {
            // We need to enable auto measure to support RecyclerView's that wrap their content
            // height.
            return true;
        }

        @Override
        public boolean canScrollHorizontally() {
            return mMaxScroll > 0;
        }

        @Override
        public void onLayoutChildren(RecyclerView.Recycler recycler, RecyclerView.State state) {
            int itemCount = state.getItemCount();
            detachAndScrapAttachedViews(recycler);
            if (itemCount == 0) {
                return;
            }

            int extraWidth = getWidth() - getPaddingLeft() - getPaddingRight() - mPaddingEnd
                    - mSpaceBetweenChildren / 2;
            extraWidth -= mSpaceBetweenChildren * (itemCount - 1);

            // Add children and measure them. The extra available width will be added after the
            // first chip if there are more than one, or will be used to center it if there is only
            // one.
            for (int i = 0; i < itemCount; i++) {
                View child = recycler.getViewForPosition(i);
                addView(child);
                measureChildWithMargins(child, 0, 0);
                int childWidth = mOrientationHelper.getDecoratedMeasurement(child);
                extraWidth -= childWidth;
            }

            // If we are missing space, allow scrolling.
            if (extraWidth < 0) {
                mMaxScroll = -extraWidth;
            } else {
                mMaxScroll = 0;
            }

            // TODO(crbug.com/806868): When chips are added/removed, this will automatically scroll
            // chips back to the initial state (with first chips visible). We might want to have a
            // smart scroll that doesn't move too much when adding/removing chips.
            mScroll = 0;

            int top = getPaddingTop();
            int right = getWidth() - getPaddingRight() - mPaddingEnd + mSpaceBetweenChildren / 2;
            // Layout all child views but the last one from right to left.
            for (int i = 0; i < getChildCount() - 1; i++) {
                View child = getChildAt(i);
                int width = mOrientationHelper.getDecoratedMeasurement(child);
                int height = mOrientationHelper.getDecoratedMeasurementInOther(child);
                int bottom = top + height;
                int left = right - width - mSpaceBetweenChildren;
                layoutDecoratedWithMargins(child, left, top, right, bottom);

                right = left;
            }

            // Layout last child on the left. We need to layout this one after the others such that
            // it sits on top of them when scrolling.
            int left = getPaddingLeft();
            if (itemCount == 1) {
                // Center it using extra available space.
                left += extraWidth / 2;
            }

            View firstChild = getChildAt(getChildCount() - 1);
            right = left + mOrientationHelper.getDecoratedMeasurement(firstChild)
                    + mSpaceBetweenChildren;
            int bottom = top + mOrientationHelper.getDecoratedMeasurementInOther(firstChild);
            layoutDecoratedWithMargins(firstChild, left, top, right, bottom);
        }

        @Override
        public int scrollHorizontallyBy(
                int dx, RecyclerView.Recycler recycler, RecyclerView.State state) {
            // dx > 0 == scroll from right to left.
            int scrollBy = dx > 0 ? Math.min(mScroll, dx) : Math.max(-(mMaxScroll - mScroll), dx);
            mScroll -= scrollBy;

            // Offset all children except the last one.
            for (int i = 0; i < getChildCount() - 1; i++) {
                getChildAt(i).offsetLeftAndRight(-scrollBy);
            }

            return scrollBy;
        }
    }
}
